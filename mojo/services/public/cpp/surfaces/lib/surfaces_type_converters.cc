// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/surfaces/surfaces_type_converters.h"

#include "cc/output/compositor_frame.h"
#include "cc/output/delegated_frame_data.h"
#include "cc/quads/draw_quad.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"

namespace mojo {

// static
surfaces::SurfaceIdPtr
TypeConverter<surfaces::SurfaceIdPtr, cc::SurfaceId>::ConvertFrom(
    const cc::SurfaceId& input) {
  surfaces::SurfaceIdPtr id(surfaces::SurfaceId::New());
  id->id = input.id;
  return id.Pass();
}

// static
cc::SurfaceId TypeConverter<surfaces::SurfaceIdPtr, cc::SurfaceId>::ConvertTo(
    const surfaces::SurfaceIdPtr& input) {
  return cc::SurfaceId(input->id);
}

// static
surfaces::ColorPtr TypeConverter<surfaces::ColorPtr, SkColor>::ConvertFrom(
    const SkColor& input) {
  surfaces::ColorPtr color(surfaces::Color::New());
  color->rgba = input;
  return color.Pass();
}

// static
SkColor TypeConverter<surfaces::ColorPtr, SkColor>::ConvertTo(
    const surfaces::ColorPtr& input) {
  return input->rgba;
}

// static
surfaces::QuadPtr TypeConverter<surfaces::QuadPtr, cc::DrawQuad>::ConvertFrom(
    const cc::DrawQuad& input) {
  surfaces::QuadPtr quad = surfaces::Quad::New();
  quad->material = static_cast<surfaces::Material>(input.material);
  quad->rect = Rect::From(input.rect);
  quad->opaque_rect = Rect::From(input.opaque_rect);
  quad->visible_rect = Rect::From(input.visible_rect);
  quad->needs_blending = input.needs_blending;
  // This is intentionally left set to an invalid value here. It's set when
  // converting an entire pass since it's an index into the pass' shared quad
  // state list.
  quad->shared_quad_state_index = -1;
  switch (input.material) {
    case cc::DrawQuad::SOLID_COLOR: {
      const cc::SolidColorDrawQuad* color_quad =
          cc::SolidColorDrawQuad::MaterialCast(&input);
      surfaces::SolidColorQuadStatePtr color_state =
          surfaces::SolidColorQuadState::New();
      color_state->color = surfaces::Color::From(color_quad->color);
      color_state->force_anti_aliasing_off =
          color_quad->force_anti_aliasing_off;
      quad->solid_color_quad_state = color_state.Pass();
      break;
    }
    case cc::DrawQuad::SURFACE_CONTENT: {
      const cc::SurfaceDrawQuad* surface_quad =
          cc::SurfaceDrawQuad::MaterialCast(&input);
      surfaces::SurfaceQuadStatePtr surface_state =
          surfaces::SurfaceQuadState::New();
      surface_state->surface =
          surfaces::SurfaceId::From(surface_quad->surface_id);
      quad->surface_quad_state = surface_state.Pass();
      break;
    }
    case cc::DrawQuad::TEXTURE_CONTENT: {
      const cc::TextureDrawQuad* texture_quad =
          cc::TextureDrawQuad::MaterialCast(&input);
      surfaces::TextureQuadStatePtr texture_state =
          surfaces::TextureQuadState::New();
      texture_state = surfaces::TextureQuadState::New();
      texture_state->resource_id = texture_quad->resource_id;
      texture_state->premultiplied_alpha = texture_quad->premultiplied_alpha;
      texture_state->uv_top_left = PointF::From(texture_quad->uv_top_left);
      texture_state->uv_bottom_right =
          PointF::From(texture_quad->uv_bottom_right);
      texture_state->background_color =
          surfaces::Color::From(texture_quad->background_color);
      Array<float> vertex_opacity(4);
      for (size_t i = 0; i < 4; ++i) {
        vertex_opacity[i] = texture_quad->vertex_opacity[i];
      }
      texture_state->vertex_opacity = vertex_opacity.Pass();
      texture_state->flipped = texture_quad->flipped;
      quad->texture_quad_state = texture_state.Pass();
      break;
    }
    default:
      NOTREACHED() << "Unsupported material " << input.material;
  }
  return quad.Pass();
}

// static
scoped_ptr<cc::DrawQuad> ConvertTo(const surfaces::QuadPtr& input,
                                   cc::SharedQuadState* sqs) {
  switch (input->material) {
    case surfaces::MATERIAL_SOLID_COLOR: {
      scoped_ptr<cc::SolidColorDrawQuad> color_quad(new cc::SolidColorDrawQuad);
      color_quad->SetAll(
          sqs,
          input->rect.To<gfx::Rect>(),
          input->opaque_rect.To<gfx::Rect>(),
          input->visible_rect.To<gfx::Rect>(),
          input->needs_blending,
          input->solid_color_quad_state->color.To<SkColor>(),
          input->solid_color_quad_state->force_anti_aliasing_off);
      return color_quad.PassAs<cc::DrawQuad>();
    }
    case surfaces::MATERIAL_SURFACE_CONTENT: {
      scoped_ptr<cc::SurfaceDrawQuad> surface_quad(new cc::SurfaceDrawQuad);
      surface_quad->SetAll(
          sqs,
          input->rect.To<gfx::Rect>(),
          input->opaque_rect.To<gfx::Rect>(),
          input->visible_rect.To<gfx::Rect>(),
          input->needs_blending,
          input->surface_quad_state->surface.To<cc::SurfaceId>());
      return surface_quad.PassAs<cc::DrawQuad>();
    }
    case surfaces::MATERIAL_TEXTURE_CONTENT: {
      scoped_ptr<cc::TextureDrawQuad> texture_quad(new cc::TextureDrawQuad);
      surfaces::TextureQuadStatePtr& texture_quad_state =
          input->texture_quad_state;
      texture_quad->SetAll(
          sqs,
          input->rect.To<gfx::Rect>(),
          input->opaque_rect.To<gfx::Rect>(),
          input->visible_rect.To<gfx::Rect>(),
          input->needs_blending,
          texture_quad_state->resource_id,
          texture_quad_state->premultiplied_alpha,
          texture_quad_state->uv_top_left.To<gfx::PointF>(),
          texture_quad_state->uv_bottom_right.To<gfx::PointF>(),
          texture_quad_state->background_color.To<SkColor>(),
          texture_quad_state->vertex_opacity.storage().data(),
          texture_quad_state->flipped);
      return texture_quad.PassAs<cc::DrawQuad>();
    }
    default:
      NOTREACHED() << "Unsupported material " << input->material;
  }
  return scoped_ptr<cc::DrawQuad>();
}

// static
surfaces::SharedQuadStatePtr
TypeConverter<surfaces::SharedQuadStatePtr, cc::SharedQuadState>::ConvertFrom(
    const cc::SharedQuadState& input) {
  surfaces::SharedQuadStatePtr state = surfaces::SharedQuadState::New();
  state->content_to_target_transform =
      Transform::From(input.content_to_target_transform);
  state->content_bounds = Size::From(input.content_bounds);
  state->visible_content_rect = Rect::From(input.visible_content_rect);
  state->clip_rect = Rect::From(input.clip_rect);
  state->is_clipped = input.is_clipped;
  state->opacity = input.opacity;
  state->blend_mode = static_cast<surfaces::SkXfermode>(input.blend_mode);
  state->sorting_context_id = input.sorting_context_id;
  return state.Pass();
}

// static
scoped_ptr<cc::SharedQuadState> ConvertTo(
    const surfaces::SharedQuadStatePtr& input) {
  scoped_ptr<cc::SharedQuadState> state(new cc::SharedQuadState);
  state->SetAll(input->content_to_target_transform.To<gfx::Transform>(),
                input->content_bounds.To<gfx::Size>(),
                input->visible_content_rect.To<gfx::Rect>(),
                input->clip_rect.To<gfx::Rect>(),
                input->is_clipped,
                input->opacity,
                static_cast<SkXfermode::Mode>(input->blend_mode),
                input->sorting_context_id);
  return state.Pass();
}

// static
surfaces::PassPtr TypeConverter<surfaces::PassPtr, cc::RenderPass>::ConvertFrom(
    const cc::RenderPass& input) {
  surfaces::PassPtr pass = surfaces::Pass::New();
  pass->id = input.id.index;
  pass->output_rect = Rect::From(input.output_rect);
  pass->damage_rect = Rect::From(input.damage_rect);
  pass->transform_to_root_target =
      Transform::From(input.transform_to_root_target);
  pass->has_transparent_background = input.has_transparent_background;
  Array<surfaces::QuadPtr> quads(input.quad_list.size());
  Array<surfaces::SharedQuadStatePtr> shared_quad_state(
      input.shared_quad_state_list.size());
  int sqs_i = -1;
  const cc::SharedQuadState* last_sqs = NULL;
  for (size_t i = 0; i < quads.size(); ++i) {
    const cc::DrawQuad& quad = *input.quad_list[i];
    quads[i] = surfaces::Quad::From(quad);
    if (quad.shared_quad_state != last_sqs) {
      sqs_i++;
      shared_quad_state[sqs_i] =
          surfaces::SharedQuadState::From(*input.shared_quad_state_list[i]);
      last_sqs = quad.shared_quad_state;
    }
    quads[i]->shared_quad_state_index = sqs_i;
  }
  // We should copy all shared quad states.
  DCHECK_EQ(static_cast<size_t>(sqs_i + 1), shared_quad_state.size());
  pass->quads = quads.Pass();
  pass->shared_quad_states = shared_quad_state.Pass();
  return pass.Pass();
}

// static
scoped_ptr<cc::RenderPass> ConvertTo(const surfaces::PassPtr& input) {
  scoped_ptr<cc::RenderPass> pass = cc::RenderPass::Create();
  pass->SetAll(cc::RenderPass::Id(1, input->id),
               input->output_rect.To<gfx::Rect>(),
               input->damage_rect.To<gfx::Rect>(),
               input->transform_to_root_target.To<gfx::Transform>(),
               input->has_transparent_background);
  cc::SharedQuadStateList& sqs_list = pass->shared_quad_state_list;
  sqs_list.reserve(input->shared_quad_states.size());
  for (size_t i = 0; i < input->shared_quad_states.size(); ++i) {
    sqs_list.push_back(ConvertTo(input->shared_quad_states[i]));
  }
  cc::QuadList& quad_list = pass->quad_list;
  quad_list.reserve(input->quads.size());
  for (size_t i = 0; i < input->quads.size(); ++i) {
    surfaces::QuadPtr quad = input->quads[i].Pass();
    quad_list.push_back(
        ConvertTo(quad, sqs_list[quad->shared_quad_state_index]));
  }
  return pass.Pass();
}

// static
surfaces::MailboxPtr
TypeConverter<surfaces::MailboxPtr, gpu::Mailbox>::ConvertFrom(
    const gpu::Mailbox& input) {
  Array<int8_t> name(64);
  for (int i = 0; i < 64; ++i) {
    name[i] = input.name[i];
  }
  surfaces::MailboxPtr mailbox(surfaces::Mailbox::New());
  mailbox->name = name.Pass();
  return mailbox.Pass();
}

// static
gpu::Mailbox TypeConverter<surfaces::MailboxPtr, gpu::Mailbox>::ConvertTo(
    const surfaces::MailboxPtr& input) {
  gpu::Mailbox mailbox;
  mailbox.SetName(input->name.storage().data());
  return mailbox;
}

// static
surfaces::MailboxHolderPtr
TypeConverter<surfaces::MailboxHolderPtr, gpu::MailboxHolder>::ConvertFrom(
    const gpu::MailboxHolder& input) {
  surfaces::MailboxHolderPtr holder(surfaces::MailboxHolder::New());
  holder->mailbox = surfaces::Mailbox::From<gpu::Mailbox>(input.mailbox);
  holder->texture_target = input.texture_target;
  holder->sync_point = input.sync_point;
  return holder.Pass();
}

// static
gpu::MailboxHolder
TypeConverter<surfaces::MailboxHolderPtr, gpu::MailboxHolder>::ConvertTo(
    const surfaces::MailboxHolderPtr& input) {
  gpu::MailboxHolder holder;
  holder.mailbox = input->mailbox.To<gpu::Mailbox>();
  holder.texture_target = input->texture_target;
  holder.sync_point = input->sync_point;
  return holder;
}

// static
surfaces::TransferableResourcePtr TypeConverter<
    surfaces::TransferableResourcePtr,
    cc::TransferableResource>::ConvertFrom(const cc::TransferableResource&
                                               input) {
  surfaces::TransferableResourcePtr transferable =
      surfaces::TransferableResource::New();
  transferable->id = input.id;
  transferable->format = static_cast<surfaces::ResourceFormat>(input.format);
  transferable->filter = input.filter;
  transferable->size = Size::From(input.size);
  transferable->mailbox_holder =
      surfaces::MailboxHolder::From(input.mailbox_holder);
  transferable->is_repeated = input.is_repeated;
  transferable->is_software = input.is_software;
  return transferable.Pass();
}

// static
cc::TransferableResource
TypeConverter<surfaces::TransferableResourcePtr, cc::TransferableResource>::
    ConvertTo(const surfaces::TransferableResourcePtr& input) {
  cc::TransferableResource transferable;
  transferable.id = input->id;
  transferable.format = static_cast<cc::ResourceFormat>(input->format);
  transferable.filter = input->filter;
  transferable.size = input->size.To<gfx::Size>();
  transferable.mailbox_holder = input->mailbox_holder.To<gpu::MailboxHolder>();
  transferable.is_repeated = input->is_repeated;
  transferable.is_software = input->is_software;
  return transferable;
}

// static
Array<surfaces::TransferableResourcePtr>
TypeConverter<Array<surfaces::TransferableResourcePtr>,
              cc::TransferableResourceArray>::
    ConvertFrom(const cc::TransferableResourceArray& input) {
  Array<surfaces::TransferableResourcePtr> resources(input.size());
  for (size_t i = 0; i < input.size(); ++i) {
    resources[i] = surfaces::TransferableResource::From(input[i]);
  }
  return resources.Pass();
}

// static
cc::TransferableResourceArray
TypeConverter<Array<surfaces::TransferableResourcePtr>,
              cc::TransferableResourceArray>::
    ConvertTo(const Array<surfaces::TransferableResourcePtr>& input) {
  cc::TransferableResourceArray resources(input.size());
  for (size_t i = 0; i < input.size(); ++i) {
    resources[i] = input[i].To<cc::TransferableResource>();
  }
  return resources;
}

// static
surfaces::ReturnedResourcePtr
TypeConverter<surfaces::ReturnedResourcePtr, cc::ReturnedResource>::ConvertFrom(
    const cc::ReturnedResource& input) {
  surfaces::ReturnedResourcePtr returned = surfaces::ReturnedResource::New();
  returned->id = input.id;
  returned->sync_point = input.sync_point;
  returned->count = input.count;
  returned->lost = input.lost;
  return returned.Pass();
}

// static
cc::ReturnedResource
TypeConverter<surfaces::ReturnedResourcePtr, cc::ReturnedResource>::ConvertTo(
    const surfaces::ReturnedResourcePtr& input) {
  cc::ReturnedResource returned;
  returned.id = input->id;
  returned.sync_point = input->sync_point;
  returned.count = input->count;
  returned.lost = input->lost;
  return returned;
}

// static
Array<surfaces::ReturnedResourcePtr> TypeConverter<
    Array<surfaces::ReturnedResourcePtr>,
    cc::ReturnedResourceArray>::ConvertFrom(const cc::ReturnedResourceArray&
                                                input) {
  Array<surfaces::ReturnedResourcePtr> resources(input.size());
  for (size_t i = 0; i < input.size(); ++i) {
    resources[i] = surfaces::ReturnedResource::From(input[i]);
  }
  return resources.Pass();
}

// static
surfaces::FramePtr
TypeConverter<surfaces::FramePtr, cc::CompositorFrame>::ConvertFrom(
    const cc::CompositorFrame& input) {
  surfaces::FramePtr frame = surfaces::Frame::New();
  DCHECK(input.delegated_frame_data);
  cc::DelegatedFrameData* frame_data = input.delegated_frame_data.get();
  frame->resources =
      Array<surfaces::TransferableResourcePtr>::From(frame_data->resource_list);
  const cc::RenderPassList& pass_list = frame_data->render_pass_list;
  frame->passes = Array<surfaces::PassPtr>::New(pass_list.size());
  for (size_t i = 0; i < pass_list.size(); ++i) {
    frame->passes[i] = surfaces::Pass::From(*pass_list[i]);
  }
  return frame.Pass();
}

// static
scoped_ptr<cc::CompositorFrame> ConvertTo(const surfaces::FramePtr& input) {
  scoped_ptr<cc::DelegatedFrameData> frame_data(new cc::DelegatedFrameData);
  frame_data->device_scale_factor = 1.f;
  frame_data->resource_list =
      input->resources.To<cc::TransferableResourceArray>();
  frame_data->render_pass_list.reserve(input->passes.size());
  for (size_t i = 0; i < input->passes.size(); ++i) {
    frame_data->render_pass_list.push_back(ConvertTo(input->passes[i]));
  }
  scoped_ptr<cc::CompositorFrame> frame(new cc::CompositorFrame);
  frame->delegated_frame_data = frame_data.Pass();
  return frame.Pass();
}

}  // namespace mojo
