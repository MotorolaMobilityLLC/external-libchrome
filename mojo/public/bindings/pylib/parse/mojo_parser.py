#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates a syntax tree from a Mojo IDL file."""


import sys
import os.path

# Try to load the ply module, if not, then assume it is in the third_party
# directory.
try:
  # Disable lint check which fails to find the ply module.
  # pylint: disable=F0401
  from ply import lex
  from ply import yacc
except ImportError:
  module_path, module_name = os.path.split(__file__)
  third_party = os.path.join(module_path, os.pardir, os.pardir, os.pardir,
                             os.pardir, os.pardir, 'third_party')
  sys.path.append(third_party)
  # pylint: disable=F0401
  from ply import lex
  from ply import yacc

from mojo_lexer import Lexer


def ListFromConcat(*items):
  """Generate list by concatenating inputs"""
  itemsout = []
  for item in items:
    if item is None:
      continue
    if type(item) is not type([]):
      itemsout.append(item)
    else:
      itemsout.extend(item)

  return itemsout


class Parser(object):

  def __init__(self, lexer):
    self.tokens = lexer.tokens

  def p_module(self, p):
    """module : MODULE NAME LBRACE definitions RBRACE"""
    p[0] = ('MODULE', p[2], p[4])

  def p_definitions(self, p):
    """definitions : definition definitions
                   | """
    if len(p) > 1:
      p[0] = ListFromConcat(p[1], p[2])

  def p_definition(self, p):
    """definition : struct
                  | interface
                  | enum"""
    p[0] = p[1]

  def p_attribute_section(self, p):
    """attribute_section : LBRACKET attributes RBRACKET
                         | """
    if len(p) > 3:
      p[0] = p[2]

  def p_attributes(self, p):
    """attributes : attribute
                  | attribute COMMA attributes
                  | """
    if len(p) == 2:
      p[0] = ListFromConcat(p[1])
    elif len(p) > 3:
      p[0] = ListFromConcat(p[1], p[3])

  def p_attribute(self, p):
    """attribute : NAME EQUALS expression
                 | NAME EQUALS NAME"""
    p[0] = ('ATTRIBUTE', p[1], p[3])

  def p_struct(self, p):
    """struct : attribute_section STRUCT NAME LBRACE struct_body RBRACE SEMI"""
    p[0] = ('STRUCT', p[3], p[1], p[5])

  def p_struct_body(self, p):
    """struct_body : field struct_body
                   | enum struct_body
                   | """
    if len(p) > 1:
      p[0] = ListFromConcat(p[1], p[2])

  def p_field(self, p):
    """field : typename NAME default ordinal SEMI"""
    p[0] = ('FIELD', p[1], p[2], p[4], p[3])

  def p_default(self, p):
    """default : EQUALS expression
               | EQUALS expression_object
               | """
    if len(p) > 2:
      p[0] = p[2]

  def p_interface(self, p):
    """interface : attribute_section INTERFACE NAME LBRACE interface_body \
                       RBRACE SEMI"""
    p[0] = ('INTERFACE', p[3], p[1], p[5])

  def p_interface_body(self, p):
    """interface_body : method interface_body
                      | enum interface_body
                      | """
    if len(p) > 1:
      p[0] = ListFromConcat(p[1], p[2])

  def p_method(self, p):
    """method : VOID NAME LPAREN parameters RPAREN ordinal SEMI"""
    p[0] = ('METHOD', p[2], p[4], p[6])

  def p_parameters(self, p):
    """parameters : parameter
                  | parameter COMMA parameters
                  | """
    if len(p) == 1:
      p[0] = []
    elif len(p) == 2:
      p[0] = ListFromConcat(p[1])
    elif len(p) > 3:
      p[0] = ListFromConcat(p[1], p[3])

  def p_parameter(self, p):
    """parameter : typename NAME ordinal"""
    p[0] = ('PARAM', p[1], p[2], p[3])

  def p_typename(self, p):
    """typename : basictypename
                | array"""
    p[0] = p[1]

  def p_basictypename(self, p):
    """basictypename : NAME
                     | HANDLE
                     | specializedhandle"""
    p[0] = p[1]

  def p_specializedhandle(self, p):
    """specializedhandle : HANDLE LT specializedhandlename GT"""
    p[0] = "handle<" + p[3] + ">"

  def p_specializedhandlename(self, p):
    """specializedhandlename : DATA_PIPE_CONSUMER
                             | DATA_PIPE_PRODUCER
                             | MESSAGE_PIPE"""
    p[0] = p[1]

  def p_array(self, p):
    """array : basictypename LBRACKET RBRACKET"""
    p[0] = p[1] + "[]"

  def p_ordinal(self, p):
    """ordinal : ORDINAL
               | """
    if len(p) > 1:
      p[0] = p[1]

  def p_enum(self, p):
    """enum : ENUM NAME LBRACE enum_fields RBRACE SEMI"""
    p[0] = ('ENUM', p[2], p[4])

  def p_enum_fields(self, p):
    """enum_fields : enum_field
                   | enum_field COMMA enum_fields
                   | """
    if len(p) == 2:
      p[0] = ListFromConcat(p[1])
    elif len(p) > 3:
      p[0] = ListFromConcat(p[1], p[3])

  def p_enum_field(self, p):
    """enum_field : NAME
                  | NAME EQUALS expression"""
    if len(p) == 2:
      p[0] = ('ENUM_FIELD', p[1], None)
    else:
      p[0] = ('ENUM_FIELD', p[1], p[3])

  ### Expressions ###

  def p_expression_object(self, p):
    """expression_object : expression_array
                         | LBRACE expression_object_elements RBRACE """
    if len(p) < 3:
      p[0] = p[1]
    else:
      p[0] = ('OBJECT', p[2])

  def p_expression_object_elements(self, p):
    """expression_object_elements : expression_object
                                 | expression_object COMMA expression_object_elements
                                 | """
    if len(p) == 2:
      p[0] = ListFromConcat(p[1])
    elif len(p) > 3:
      p[0] = ListFromConcat(p[1], p[3])

  def p_expression_array(self, p):
    """expression_array : expression
                        | LBRACKET expression_array_elements RBRACKET """
    if len(p) < 3:
      p[0] = p[1]
    else:
      p[0] = ('ARRAY', p[2])

  def p_expression_array_elements(self, p):
    """expression_array_elements : expression_object
                                 | expression_object COMMA expression_array_elements
                                 | """
    if len(p) == 2:
      p[0] = ListFromConcat(p[1])
    elif len(p) > 3:
      p[0] = ListFromConcat(p[1], p[3])

  def p_expression(self, p):
    """expression : conditional_expression"""
    p[0] = p[1]

  def p_conditional_expression(self, p):
    """conditional_expression : binary_expression
                              | binary_expression CONDOP expression COLON \
                                    conditional_expression"""
    # Just pass the arguments through. I don't think it's possible to preserve
    # the spaces of the original, so just put a single space between them.
    p[0] = ' '.join(p[1:])

  # PLY lets us specify precedence of operators, but since we don't actually
  # evaluate them, we don't need that here.
  def p_binary_expression(self, p):
    """binary_expression : unary_expression
                         | binary_expression binary_operator \
                               binary_expression"""
    p[0] = ' '.join(p[1:])

  def p_binary_operator(self, p):
    """binary_operator : TIMES
                       | DIVIDE
                       | MOD
                       | PLUS
                       | MINUS
                       | RSHIFT
                       | LSHIFT
                       | LT
                       | LE
                       | GE
                       | GT
                       | EQ
                       | NE
                       | AND
                       | OR
                       | XOR
                       | LAND
                       | LOR"""
    p[0] = p[1]

  def p_unary_expression(self, p):
    """unary_expression : primary_expression
                        | unary_operator expression"""
    p[0] = ''.join(p[1:])

  def p_unary_operator(self, p):
    """unary_operator : PLUS
                      | MINUS
                      | NOT
                      | LNOT"""
    p[0] = p[1]

  def p_primary_expression(self, p):
    """primary_expression : constant
                          | NAME
                          | LPAREN expression RPAREN"""
    p[0] = ''.join(p[1:])

  def p_constant(self, p):
    """constant : INT_CONST_DEC
                | INT_CONST_OCT
                | INT_CONST_HEX
                | FLOAT_CONST
                | HEX_FLOAT_CONST
                | CHAR_CONST
                | WCHAR_CONST
                | STRING_LITERAL
                | WSTRING_LITERAL"""
    p[0] = ''.join(p[1:])

  def p_error(self, e):
    print('error: %s'%e)


def Parse(filename):
  lexer = Lexer()
  parser = Parser(lexer)

  lex.lex(object=lexer)
  yacc.yacc(module=parser, debug=0, write_tables=0)

  tree = yacc.parse(open(filename).read())
  return tree


def Main():
  if len(sys.argv) < 2:
    print("usage: %s filename" % (sys.argv[0]))
    sys.exit(1)
  tree = Parse(filename=sys.argv[1])
  print(tree)


if __name__ == '__main__':
  Main()
