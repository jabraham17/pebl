import ctypes
import enum
from typing import Iterator, Optional, List, Type, Union
import clayer


class OpaqueStructure(ctypes.Structure):
    pass


OpaqueStructurePtr = ctypes.POINTER(OpaqueStructure)


class cAstNode(ctypes.Structure):
    pass


cAstNodePtr = ctypes.POINTER(cAstNode)
cAstNodePtrType = Type[cAstNodePtr]
cAstNode._fields_ = [
    ("at", ctypes.c_int),
    ("next", cAstNodePtr),
    ("children", cAstNodePtr * 4),
    ("int_value", ctypes.c_int),
    ("str_value", ctypes.c_char_p),
]


class cLocation(ctypes.Structure):
    pass


cLocationPtr = ctypes.POINTER(cLocation)
cLocation._fields_ = [
    ("ast", cAstNodePtr),
    ("line_start", ctypes.c_int),
    ("line_end", ctypes.c_int),
    ("next", cLocationPtr),
]


class cContext(ctypes.Structure):
    _fields_ = [
        ("filename", ctypes.c_char_p),
        ("lexer_state", OpaqueStructurePtr),
        ("ast", cAstNodePtr),
        ("locations", OpaqueStructurePtr),
        ("types", OpaqueStructurePtr),
        ("scope_table", OpaqueStructurePtr),
        ("compiler_builtins", OpaqueStructurePtr),
        ("codegen", OpaqueStructurePtr),
    ]


cContextPtr = ctypes.POINTER(cContext)

clayer.lib.Context_allocate.restype = cContextPtr
clayer.lib.ast_get_child.restype = cAstNodePtr
clayer.lib.ast_next.restype = cAstNodePtr
clayer.lib.ast_Identifier_name.restype = ctypes.c_char_p
clayer.lib.Context_get_location.restype = cLocationPtr


class Context:
    def __init__(self, filename: str):
        self._instance = clayer.lib.Context_allocate()
        cfilename = filename.encode("utf-8")
        clayer.lib.Context_init(self._instance, ctypes.c_char_p(cfilename))

    def parse(self):
        clayer.lib.lexer_init(self._instance)
        clayer.lib.parser_init(self._instance)
        clayer.lib.parser_parse(self._instance)
        clayer.lib.lexer_deinit(self._instance)

    def dump(self):
        clayer.lib.dump_ast(self._instance)

    def ast(self) -> Union["AstNode", None]:
        if p := c._instance.contents.ast:
            ast = AstNode.create(p)
            return ast


class AstType(enum.IntEnum):
    Identifier = 0
    Typename = enum.auto()
    FieldAccess = enum.auto()
    Type = enum.auto()
    Variable = enum.auto()
    Function = enum.auto()
    Assignment = enum.auto()
    Return = enum.auto()
    Break = enum.auto()
    Block = enum.auto()
    Conditional = enum.auto()
    While = enum.auto()
    Expr = enum.auto()
    Call = enum.auto()
    Number = enum.auto()
    String = enum.auto()


class AstNode:
    def __init__(self, instance: cAstNodePtrType, parent: Optional["AstNode"] = None):
        self._instance = instance
        self._parent = parent

    def node_type(self) -> AstType:
        return AstType(self._instance.contents.at)

    def __iter__(self) -> Iterator["AstNode"]:
        p = self
        while p:
            yield p
            if n := p._instance.contents.next:
                p = AstNode.create(n, p._parent)
            else:
                p = None

    def children(self) -> Iterator["AstNode"]:
        for c in self._instance.contents.children:
            if c:
                yield AstNode.create(c, self)

    def parent(self) -> Optional["AstNode"]:
        return self._parent

    def lineno(self, ctx) -> int:
        if loc := clayer.lib.Context_get_location(ctx._instance, self._instance):
            return loc.contents.line_start
        return None

    def __str__(self) -> str:
        return f"AstNode<{self.node_type().name}>"

    @classmethod
    def create(
        cls, instance: cAstNodePtrType, parent: Optional["AstNode"] = None
    ) -> "AstNode":
        assert instance
        if instance.contents.at == AstType.Identifier:
            return AstNodeIdentifier(instance, parent)
        return AstNode(instance, parent)

    @classmethod
    def preorder(cls, node: "AstNode") -> Iterator["AstNode"]:
        for n in node:
            yield n
            for c in n.children():
                yield from AstNode.preorder(c)


clayer.lib.scope_resolve.restype = OpaqueStructurePtr
clayer.lib.scope_lookup_identifier.restype = OpaqueStructurePtr
clayer.lib.ScopeSymbol_name.restype = ctypes.c_char_p


class AstNodeIdentifier(AstNode):
    def value(self) -> str:
        return clayer.lib.ast_Identifier_name(self._instance).decode("utf-8")

    def find_def(self, ctx: Context) -> Optional[AstNode]:
        p = self.parent()
        while p:
            if p.node_type() == AstType.Function or p.node_type() == AstType.Block:
                break
            p = p.parent()
        res = clayer.lib.scope_resolve(ctx._instance, p._instance)
        ss = clayer.lib.scope_lookup_identifier(ctx._instance, res, self._instance, 1)
        if ss:
            print(str(clayer.lib.ScopeSymbol_name(ss)))
        return None

    def __str__(self) -> str:
        return super().__str__() + f" name='{self.value()}'"


c = Context("foo.pebl")
print(c)
c.parse()
c.dump()
ast = c.ast()
if not ast:
    exit()
for n in AstNode.preorder(ast):
    if n.node_type() == AstType.Identifier:
        print(n)
        n.find_def(c)
