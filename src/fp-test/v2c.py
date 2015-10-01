#!/usr/bin/env python3

# This is a big hack to translate the algorithm from a Verilog module to C.
# Don't expect this to work for any modules other than the ones it was
# intended for...

import re

class ScanError(BaseException):
    def __init__(self, scanner):
        self.where = scanner.where()


class Scanner():
    KEYWORDS = {
        'module', 'input', 'output', 'reg', 'wire', 'assign',
        'always', 'posedge', 'begin', 'end', 'endmodule',
    }

    SCAN_RE = re.compile(r'''
      (?P<word> [_a-zA-Z][a-zA-Z0-9]* ) |
      (?P<bits> (?P<bitlen>[0-9]+) 'b (?P<bitdata>[0-1]+) ) |
      (?P<hex>  (?P<hexlen>[0-9]+) 'h (?P<hexdata>[0-9A-Fa-f]+) ) |
      (?P<int>  [0-9]+ ) |
      (?P<sym>  == | <= | [();\[:\]=^+-?{}~&|@] ) |
      \Z
    ''', re.X)

    SKIP_RE = re.compile(r'''
      (?: //.* | `.* | \s+ )*
    ''', re.X)

    def __init__(self, string):
        self.string = string
        self.pos = 0
        self.next_pos = self.SKIP_RE.match(self.string, pos=self.pos).end()

    def __iter__(self):
        return self

    def __next__(self):
        self.value = None
        while True:
            self.pos = self.next_pos
            match = self.SCAN_RE.match(self.string, pos=self.pos)
            if match is None:
                raise ScanError(self)
            self.next_pos = self.SKIP_RE.match(self.string, pos=match.end()).end()

            if match.group('word'):
                w = match.group('word')
                return (w, None) if w in self.KEYWORDS else ('<name>', w)
            elif match.group('bits'):
                bitlen = int(match.group('bitlen'))
                bitdata = int(match.group('bitdata'), 2)
                return ('<bits>', bits(size=bitlen, value=bitdata))
            elif match.group('hex'):
                hexlen = int(match.group('hexlen'))
                hexdata = int(match.group('hexdata'), 16)
                return ('<bits>', bits(size=hexlen, value=hexdata))
            elif match.group('int'):
                return ('<int>', int(match.group('int')))
            elif match.group('sym'):
                return (match.group('sym'), None)
            else:
                raise StopIteration

    def where(self):
        line = self.string.count('\n', 0, self.pos) + 1
        col = self.pos - self.string.rfind('\n', 0, self.pos)
        return (line, col)


class bits:
    def __init__(self, *, size, value):
        self.size = size
        self.value = value

    def __str__(self):
        return '<%d bits: %s>' % (self.size, self.value)


class ParseError(BaseException):
    pass


class Parser():
    def __init__(self, text):
        self.names = {}
        self.registers = {}
        self.bit_by_bit = {}
        self.scanner = Scanner(text)
        self.token, self.value = None, None
        self.next_token()

    def next_token(self):
        self.token, self.value = next(self.scanner)

    def expected(self, *tokens):
        raise ParseError("at %s: expected %s, found %s" %
                         (self.scanner.where(), tokens, self.token))

    def skip_over(self, token):
        if self.token != token:
            self.expected(token)
        self.next_token()

    def skipping_over(self, *tokens):
        if self.token in tokens:
            self.next_token()
            return True

    def name(self):
        n = self.value
        self.skip_over('<name>')
        return n

    def int(self):
        n = self.value
        self.skip_over('<int>')
        return n

    def skip_over_name(self, name):
        n = self.name()
        if n != name:
            self.expected(name)

    def parse_program(self):
        self.parse_module()
        while self.token != 'endmodule':
            if self.skipping_over('reg'):
                self.parse_reg()
            elif self.skipping_over('wire'):
                self.parse_wire()
            elif self.skipping_over('assign'):
                self.parse_assign()
            elif self.skipping_over('always'):
                self.parse_always()
            else:
                raise ParseError('wtf %s' % self.token)
        print("static void cycle() {")
        for n, expr in self.registers.items():
            size = self.names[n].size
            print("  %s %s_tmp = (%s) & %s;" % (
                self.get_type(size), n, expr.value, self.mask(size)))
        for n, expr in self.registers.items():
            print("  %s = %s_tmp;" % (n, n))
        print("}")

    def get_type(self, size):
        return 'bool' if size == 1 else 'uint64_t'

    def declare_input(self, name, size):
        type = self.get_type(size)
        self.names[name] = bits(size=size, value=name)
        print('static %s %s;' % (type, name))

    def declare_wire(self, name, size):
        type = self.get_type(size)
        self.names[name] = bits(size=size, value=name+'()')
        print('static %s %s(void);' % (type, name))

    def declare_reg(self, name, size):
        type = self.get_type(size)
        self.names[name] = bits(size=size, value=name)
        print('static %s %s;' % (type, name))

    def parse_module(self):
        self.skip_over('module')
        self.skip_over('<name>')
        self.skip_over('(')
        while not self.skipping_over('endmodule'):
            if self.skipping_over(')'):
                break
            elif self.skipping_over('input'):
                size = self.parse_decl_bitlen()
                while self.token == '<name>':
                    name = self.value
                    self.declare_input(name, size)
                    self.next_token()
                    if not self.skipping_over(','):
                        break
            elif self.skipping_over('output'):
                size = self.parse_decl_bitlen()
                while self.token == '<name>':
                    name = self.value
                    self.declare_wire(name, size)
                    self.next_token()
                    if not self.skipping_over(','):
                        break
            else:
                raise ParseError("don't understand module statement: %s" % self.token)
        self.skip_over(';')

    def parse_decl_bitlen(self):
        if self.skipping_over('['):
            n1 = self.int();
            self.skip_over(':')
            n2 = self.int();
            self.skip_over(']')
            if n2 != 0:
                raise ParseError("end of range is not 0: %s,%s" % (n1,n2))
            return n1 + 1
        else:
            return 1

    def parse_wire(self):
        size = self.parse_decl_bitlen()
        while True:
            name = self.name()
            self.declare_wire(name, size)
            if self.skipping_over(';'):
                break
            self.skip_over(',')

    def parse_reg(self):
        size = self.parse_decl_bitlen()
        while True:
            name = self.name()
            self.declare_reg(name, size)
            if self.skipping_over(';'):
                break
            self.skip_over(',')

    def parse_assign(self):
        name = self.name()
        size = self.names[name].size
        if self.skipping_over('['):
            self.parse_assign_bit_by_bit(name)
        else:
            self.skip_over('=')
            a = self.parse_expression()
            self.skip_over(';')
            print('static %s %s(void) { return (%s) & %s; }' %
                  (self.get_type(size), name, a.value, self.mask(size)))

    def parse_assign_bit_by_bit(self, name):
        size = self.names[name].size
        if name not in self.bit_by_bit:
            self.bit_by_bit[name] = [None] * size
        idx = self.int()
        self.skip_over(']')
        self.skip_over('=')
        a = self.parse_expression()
        if a.size != 1:
            raise ParseError("Expected a 1-bit expression")
        self.skip_over(';')
        self.bit_by_bit[name][idx] = a.value
        if all(b is not None for b in self.bit_by_bit[name]):
            parts = ['((%s & 1) << %s)' % (self.bit_by_bit[name][i], i) for i in range(size)]
            expr = '(%s)' % ' | '.join(reversed(parts))
            print('static %s %s(void) { return %s; }' % (self.get_type(size), name, expr))

    def parse_always(self):
        self.skip_over('@')
        self.skip_over('(')
        self.skip_over('posedge')
        self.skip_over('(')
        self.skip_over('<name>')
        self.skip_over(')')
        self.skip_over(')')
        if self.skipping_over('begin'):
            while not self.skipping_over('end'):
                self.parse_reg_assign()
        else:
            self.parse_reg_assign()

    def parse_reg_assign(self):
        n = self.name()
        self.skip_over('<=')
        e = self.parse_expression()
        self.skip_over(';')
        self.registers[n] = e;

    def parse_expression(self):
        return self.parse_expr_trinary()

    def parse_expr_trinary(self):
        a = self.parse_expr_or()
        if not self.skipping_over('?'):
            return a
        b = self.parse_expr_trinary()
        self.skip_over(':')
        c = self.parse_expr_trinary()
        size = self.size(b, c)
        value="(%s ? %s : %s)" % (a.value, b.value, c.value)
        return bits(size=size, value=value)

    def parse_expr_or(self):
        a = self.parse_expr_xor()
        while self.skipping_over('|'):
            b = self.parse_expr_xor()
            size = self.size(a, b)
            value = "(%s | %s)" % (a.value, b.value)
            a = bits(size=size, value=value)
        return a

    def parse_expr_xor(self):
        a = self.parse_expr_and()
        while self.skipping_over('^'):
            b = self.parse_expr_and()
            size = self.size(a, b)
            value = "(%s ^ %s)" % (a.value, b.value)
            a = bits(size=size, value=value)
        return a

    def parse_expr_and(self):
        a = self.parse_expr_eq()
        while self.skipping_over('&'):
            b = self.parse_expr_eq()
            size = self.size(a, b)
            value = "(%s & %s)" % (a.value, b.value)
            a = bits(size=size, value=value)
        return a

    def parse_expr_eq(self):
        a = self.parse_expr_plus()
        while self.skipping_over('=='):
            b = self.parse_expr_plus()
            value = "(%s == %s)" % (a.value, b.value)
            a = bits(size=1, value=value)
        return a

    def parse_expr_plus(self):
        a = self.parse_expr_concat()
        while True:
            op = self.token
            if not self.skipping_over('+', '-'):
                return a
            b = self.parse_expr_concat()
            size = self.size(a, b)
            value = "(%s %s %s)" % (a.value, op, b.value)
            a = bits(size=size, value=value)

    def parse_expr_concat(self):
        if not self.skipping_over('{'):
            return self.parse_expr_unary_plus()
        exprs = []
        while True:
            if self.token == '<int>':
                exprs.append(self.parse_expr_repeat())
            else:
                exprs.append(self.parse_expression())
            if not self.skipping_over(','):
                break
        self.skip_over('}')
        size = 0
        parts = []
        for b in reversed(exprs):
            parts.append('((%s & %s) << %s)' % (b.value, self.mask(b.size), size))
            size += b.size
        return bits(size=size, value='(%s)' % ' | '.join(reversed(parts)))

    def parse_expr_repeat(self):
        count = self.int()
        self.skip_over('{')
        expr = self.parse_expr_number()
        self.skip_over('}')
        if expr.size != 1:
            raise ParseError("Can only repeat 1-bit values")
        parts = []
        for i in range(count):
            parts.append('((%s & 1) << %s)' % (expr.value, i))
        return bits(size=count, value='(%s)' % ' | '.join(parts))

    def parse_expr_unary_plus(self):
        if self.skipping_over('+') or not self.skipping_over('-'):
            return self.parse_expr_negation()
        a = self.parse_expr_number()
        value = "(-%s)" % (a.value,)
        return bits(size=a.size, value=value)

    def parse_expr_negation(self):
        if not self.skipping_over('~'):
            return self.parse_expr_number()
        a = self.parse_expr_number()
        value = "((~%s) & %s)" % (a.value, self.mask(a.size))
        return bits(size=a.size, value=value)

    def parse_expr_number(self):
        if self.skipping_over('('):
            a = self.parse_expression()
            self.skip_over(')')
        elif self.token == '<int>':
            a = bits(value=self.value, size=None)
            self.next_token()
        elif self.token == '<bits>':
            a = self.value
            self.next_token()
        elif self.token == '<name>':
            a = self.names[self.value]
            self.next_token()
        else:
            raise ParseError("Don't now what to do with %s" % self.token)

        if self.skipping_over('['):
            n1 = self.int()
            if self.skipping_over(':'):
                n2 = self.int()
            else:
                n2 = n1
            self.skip_over(']')
            if n1 < n2:
                raise ParseError("bogus range %s" % ((n1,n2),))
            if n1 >= a.size:
                raise ParseError("range %s out of bound in %s" % ((n1,n2), a))
            rsize = n1 - n2 + 1
            rvalue = '((%s >> %d) & %s)' % (a.value, n2, self.mask(n1-n2+1))
            a = bits(size=rsize, value=rvalue)
        return a

    def mask(self, n):
        if n is None: n = 32
        return '0x%XULL' % ((1<<n)-1,)

    def size(self, a, b):
        if a.size is None: return b.size
        if b.size is None: return a.size
        return max(a.size, b.size)


if __name__ == '__main__':
    import sys
    Parser(sys.stdin.read()).parse_program()
