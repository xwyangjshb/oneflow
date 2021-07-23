# python3 -m pip install isort autoflake astpretty black
# requires python3.9 to run
import os
import argparse
import ast
from posixpath import basename, relpath
import subprocess
import multiprocessing
from pathlib import Path, PurePosixPath
from functools import partial
from collections import OrderedDict
import astpretty
import sys
import copy

parser = argparse.ArgumentParser()
parser.add_argument(
    "--out_dir", type=str, default="python",
)
parser.add_argument("--verbose", "-v", action="store_true")
parser.add_argument("--debug", "-d", action="store_true")
parser.add_argument("--autoflake", "-a", action="store_true")
parser.add_argument("--black", "-b", action="store_true")
parser.add_argument("--isort", "-i", action="store_true")
parser.add_argument("--save_ast", "--ast", action="store_true")
args = parser.parse_args()

OUT_PATH = Path(args.out_dir)
SAVE_AST = args.save_ast
COMPATIBLE_MODULE = "oneflow.compatible.single_client"


def dumpprint(node):
    astpretty.pprint(node)


def is_decorator(d, name=None):
    return (isinstance(d, ast.Name) and d.id == name) or (
        isinstance(d, ast.Call) and isinstance(d.func, ast.Name) and d.func.id == name
    )


def is_stable(node: ast.AST):
    for d in node.decorator_list:
        if is_decorator(d, "stable_api"):
            return True
    return False


def is_experimental(node: ast.AST):
    for d in node.decorator_list:
        if is_decorator(d, "experimental_api"):
            return True
    return False


def get_parent_module(value):
    return ".".join(value.split(".")[0:-1])


def join_module(parent, child):
    if child:
        return ".".join([parent, child])
    else:
        return parent


def path_from_module(module, is_init=False):
    if is_init:
        return Path("/".join(module.split(".") + ["__init__.py"]))
    else:
        return Path("/".join(module.split(".")) + ".py")


def module_from_path(path: Path):
    assert path.name.endswith(".py")
    parts = path.parts
    if parts[-1] == "__init__.py":
        return ".".join(path.parts[0:-1])
    else:
        return ".".join(path.parts)[0:-3]


def is_compatible_root_module(module: str):
    if module == COMPATIBLE_MODULE:
        return True
    assert module == "oneflow"
    return False


class ReservedKeywordsVisitor(ast.NodeVisitor):
    def __init__(self, keywords=None) -> None:
        self.keywords = keywords
        self.has_reserved_keyword = False

    def visit_Name(self, node: ast.Name):
        if node.id in self.keywords:
            self.has_reserved_keyword = True


class ExportVisitor(ast.NodeTransformer):
    def __init__(self, root_module="oneflow", src_target_module: str = None) -> None:
        super().__init__()
        self.staging_decorators = []
        self.root_module = root_module
        self.export_modules = {}
        self.top_imports = []
        self.src_target_module = src_target_module

    def append_export(self, target_module=None, node=None):
        if target_module not in self.export_modules:
            module = ast.Module(body=[], type_ignores=[])
            self.export_modules[target_module] = module
        else:
            module = self.export_modules[target_module]
        # dumpprint(module)
        if isinstance(node, list):
            module.body += node
        else:
            module.body.append(node)

    def visit_Expr(self, node):
        if isinstance(node.value, ast.Constant):
            if "Copyright 2020 The OneFlow Authors" in node.value.value:
                return None
        return node

    def visit_ImportFrom(self, node):
        for name in node.names:
            if not self.visit(name):
                return None
        if node.module:
            if node.module == "__future__" or node.module.startswith(
                "oneflow.python.oneflow_export"
            ):
                return None
            if node.module.startswith("oneflow.python."):
                node.module = node.module.replace("oneflow.python.", "oneflow.")
        self.top_imports.append(node)
        return node

    def visit_Import(self, node):
        for name in node.names:
            if not super().visit(name):
                return None
        self.top_imports.append(node)
        return node

    def visit_alias(self, node: ast.alias) -> ast.alias:
        if node.name.startswith("oneflow.python."):
            node.name = node.name.replace("oneflow.python.", "oneflow.")
            return node
        elif node.name == "oneflow.python":
            node.name = "oneflow"
        elif node.name == "oneflow_export":
            return None
        elif "__export_symbols__" in node.name:
            return None
        else:
            return node

    def visit_Name(self, node: ast.AST):
        if node.id == "oneflow_export":
            return None
        if node.id == "stable_api":
            return None
        if node.id == "experimental_api":
            return None
        return node

    def visit_Call(self, node: ast.AST):
        if not self.visit(node.func):
            return None
        return node

    def visit_ClassDef(self, node):
        return self.visit_FunctionDef(node)

    def visit_FunctionDef(self, node):
        if is_compatible_root_module(self.root_module) and is_experimental(node):
            return None
        if not is_compatible_root_module(self.root_module) and is_stable(node):
            return None
        compact_decorator_list = [self.visit(d) for d in node.decorator_list]
        compact_decorator_list = [d for d in compact_decorator_list if d]
        rkv = ReservedKeywordsVisitor(keywords=set({"int", "float"}))
        rkv.visit(node)
        has_reserved_keyword = rkv.has_reserved_keyword
        is_deprecated = False
        for d in node.decorator_list:
            if is_decorator(d, name="oneflow_deprecate"):
                is_deprecated = True
        for d in node.decorator_list:
            # TODO: if @register_tensor_op, export it in __init__.py
            if is_decorator(d, name="oneflow_export"):
                is_kept_in_src = (
                    True or
                    has_reserved_keyword
                    or self.src_target_module == target_module
                    or target_module in ["oneflow", "oneflow.scope", COMPATIBLE_MODULE]
                )
                arg0 = d.args[0]
                target_module0 = join_module(
                    self.root_module, get_parent_module(arg0.value)
                )
                target_symbol0 = arg0.value.split(".")[-1]

                if is_kept_in_src:
                    target_module = self.src_target_module
                    target_symbol = node.name
                else:
                    target_module = target_module0
                    target_symbol = target_symbol0
                # nth export: import from first export
                for argN in d.args[1::]:
                    target_moduleN = join_module(
                        self.root_module, get_parent_module(argN.value)
                    )
                    target_nameN = target_name = argN.value.split(".")[-1]
                    assert arg0 != argN, {"arg0": arg0, "argN": argN}
                    import_from_first_export = ast.ImportFrom(
                        module=target_module,
                        names=[ast.alias(name=target_symbol, asname=target_nameN),],
                        level=0,
                    )
                    self.append_export(
                        target_module=target_moduleN, node=import_from_first_export
                    )

                if is_deprecated:
                    import_oneflow_deprecate = ast.ImportFrom(
                        module="oneflow",
                        names=[ast.alias(name="oneflow_deprecate")],
                        level=0,
                    )

                node.decorator_list = compact_decorator_list
                if is_kept_in_src:
                    import_from_src = ast.ImportFrom(
                        module=self.src_target_module,
                        names=[ast.alias(name=node.name, asname=target_symbol),],
                        level=0,
                    )
                    self.append_export(
                        target_module=target_module0, node=import_from_src
                    )
                    if is_deprecated:
                        return [import_oneflow_deprecate, node]
                    else:
                        return node
                else:
                    if is_deprecated:
                        self.append_export(
                            target_module=target_module, node=import_oneflow_deprecate
                        )
                    # prepend imports in target module
                    self.append_export(
                        target_module=target_module, node=self.top_imports
                    )
                    if target_module != "oneflow":
                        import_star_from_src = ast.ImportFrom(
                            module=self.src_target_module,
                            names=[ast.alias(name="*")],
                            level=0,
                        )
                        # node.body.insert(0, import_star_from_src)
                        self.append_export(
                            target_module=target_module, node=import_star_from_src
                        )
                    # save func name for src import as before modifing node.name
                    src_asname = None
                    if node.name != target_symbol:
                        src_asname = node.name
                    # save first export in target module
                    node.name = target_symbol
                    self.append_export(target_module=target_module, node=node)

                    # src: import from first export
                    return ast.ImportFrom(
                        module=target_module,
                        names=[ast.alias(name=target_symbol, asname=src_asname),],
                        level=0,
                    )
            if is_decorator(d, name="oneflow_export_value"):
                assert len(node.body) == 2
                assert len(d.args) == 1
                target_module = join_module(
                    self.root_module, get_parent_module(d.args[0].value)
                )
                call = node.body[1].value
                assign = ast.Assign(
                    targets=[
                        ast.Name(id=d.args[0].value.split(".")[-1], ctx=ast.Store())
                    ],
                    value=call,
                )
                self.append_export(target_module=target_module, node=assign)
                # TODO: the doc is not dumped properly
                # doc = node.body[0]
                # self.append_export(target_module=target_module, node=doc)
                return None
        return node


class SrcFile:
    def __init__(self, spec) -> None:
        is_test = "is_test" in spec and spec["is_test"]
        self.export_visitor = None
        self.tree = None
        self.dst = Path(spec["dst"])
        self.src: Path = spec["src"]
        self.target_module = module_from_path(self.dst)
        if is_test and args.verbose:
            print("[skip test]", self.src)
        else:
            txt = self.src.read_text()
            self.tree = ast.parse(txt)
            root_module = "oneflow"
            if (
                "compatible_single_client_python" in self.src.parts
                or self.src.name == "single_client_init.py"
                or self.src.name == "single_client_main.py"
            ):
                root_module = COMPATIBLE_MODULE
            self.export_visitor = ExportVisitor(
                root_module=root_module, src_target_module=self.target_module
            )
            self.export_visitor.visit(self.tree)


def get_specs_under_python(python_path=None, dst_path=None):
    specs = []
    for p in Path(python_path).rglob("*.py"):
        if p.name == "version.py":
            continue
        rel = p.relative_to(python_path)
        dst = Path(dst_path).joinpath(rel)
        spec = {"src": p, "dst": dst}
        if rel.parts[0] == "test":
            spec["is_test"] = True
        specs.append(spec)
    return specs


def get_files():
    srcs = (
        get_specs_under_python(python_path="oneflow/python", dst_path="oneflow")
        + get_specs_under_python(
            python_path="oneflow/compatible_single_client_python",
            dst_path="oneflow/compatible/single_client",
        )
        + [
            {"src": Path("oneflow/init.py"), "dst": "oneflow/__init__.py"},
            {"src": Path("oneflow/__main__.py"), "dst": "oneflow/__main__.py"},
            {
                "src": Path("oneflow/single_client_init.py"),
                "dst": "oneflow/compatible/single_client/__init__.py",
            },
            {
                "src": Path("oneflow/single_client_main.py"),
                "dst": "oneflow/compatible/single_client/__main__.py",
            },
        ]
    )
    srcs = list(filter(lambda x: ("oneflow_export" not in x["src"].name), srcs))
    if args.debug:
        srcs = [
            {
                "src": Path("oneflow/python/ops/nn_ops.py"),
                "dst": "oneflow/ops/nn_ops.py",
            },
            {
                "src": Path("oneflow/python/advanced/distribute_ops.py"),
                "dst": "oneflow/advanced/distribute_ops.py",
            },
        ]
    pool = multiprocessing.Pool()
    srcs = pool.map(SrcFile, srcs,)
    pool.close()
    return srcs


class ModuleNode:
    def __init__(self, name=None, parent=None) -> None:
        self.children = dict()
        self.parent = parent
        self.level = 0
        if parent:
            self.level = parent.level + 1
        self.name = name

    def add_or_get_child(self, name):
        if name in self.children:
            return self.children[name]
        else:
            self.children[name] = ModuleNode(name=name, parent=self)
            return self.children[name]

    @property
    def is_leaf(self):
        return len(self.children.keys()) == 0

    def walk(self, cb):
        cb(self)
        for child in self.children.values():
            child.walk(cb)

    @property
    def leafs(self):
        ret = []

        def add_leafs(node: ModuleNode):
            if node.is_leaf:
                ret.append(node)

        self.walk(add_leafs)
        return ret

    @property
    def full_name(self):
        current_parent = self
        ret = self.name
        while current_parent.parent:
            current_parent = current_parent.parent
            ret = current_parent.name + "." + ret
        return ret

    def __str__(self) -> str:
        return "\n".join(
            [f"{self.full_name}"]
            + [child.__str__() for child in self.children.values()]
        )

    @staticmethod
    def add_sub_module(root=None, module=None):
        parts = module.split(".")
        current_node = root
        assert current_node.name == parts[0]
        for part in parts[1::]:
            current_node = current_node.add_or_get_child(part)


def save_trees(args=None):
    dst: Path = args["dst"]
    trees = args["trees"]
    # if len(trees) > 2:
    # print(dst, len(trees))
    dst_full = OUT_PATH.joinpath(dst)
    dst_full.parent.mkdir(parents=True, exist_ok=True)
    dst_full.touch(exist_ok=False)
    # TODO: append "doctest.testmod(raise_on_error=True)"
    trees = [ast.fix_missing_locations(tree) for tree in trees]
    if SAVE_AST:
        new_txt = "\n".join([str(astpretty.pformat(tree)) for tree in trees])
        new_txt = f"""from ast import *
{new_txt}
"""
        dst_full.with_suffix(".ast.py").write_text(new_txt)
    new_txt = "\n".join([ast.unparse(tree) for tree in trees])
    dst_full.write_text(new_txt)


def append_trees(tree_dict: dict, module: str, tree: ast.AST):
    tree_dict[module] = tree_dict.get(module, [])
    tree_dict[module].append(tree)


if __name__ == "__main__":
    out_oneflow_dir = os.path.join(args.out_dir, "*")
    assert args.out_dir
    assert args.out_dir != "~"
    assert args.out_dir != "/"
    subprocess.check_call(f"mkdir -p {OUT_PATH}", shell=True)
    subprocess.check_call(f"rm -rf {out_oneflow_dir}", shell=True)
    # step 0: parse and load all segs into memory
    srcs = get_files()
    final_trees = {}

    root_module = ModuleNode(name="oneflow")
    src_module_added = {}
    for s in srcs:
        # src
        append_trees(tree_dict=final_trees, module=s.target_module, tree=s.tree)
        if (
            str(s.src) == "oneflow/python/__init__.py"
            or str(s.src) == "oneflow/compatible_single_client_python/__init__.py"
        ):
            assert not s.src.read_text()
            continue
        print("[src]", s.target_module, "<=", s.src)
        assert s.target_module not in src_module_added, {
            "target_module": s.target_module,
            "new": str(s.src),
            "exist": str(src_module_added[s.target_module]),
        }
        src_module_added[s.target_module] = s.src
        ModuleNode.add_sub_module(root=root_module, module=s.target_module)
    for s in srcs:
        # exports
        for export_path, export_tree in s.export_visitor.export_modules.items():
            print("[export]", export_path)
            append_trees(tree_dict=final_trees, module=export_path, tree=export_tree)
            ModuleNode.add_sub_module(root=root_module, module=export_path)
    # print(root_module)
    leaf_modules = set([leaf.full_name for leaf in root_module.leafs])
    pool = multiprocessing.Pool()

    print("leaf_modules", leaf_modules)

    def is_init(module: str):
        is_leaf = module in leaf_modules
        is_magic = module.endswith("__")
        if is_magic:
            print("[magic]", module)
        if is_leaf == False and is_magic == False:
            print("[init]", module)
        return is_leaf == False and is_magic == False

    srcs = pool.map(
        save_trees,
        [
            {"dst": path_from_module(module, is_init=is_init(module)), "trees": trees,}
            for module, trees in final_trees.items()
        ],
    )
    pool.close()
    # TODO: touch __init__.py, oneflow/F/__init__.py
    Path(os.path.join(OUT_PATH, "oneflow/F")).mkdir(exist_ok=True)
    Path(os.path.join(OUT_PATH, "oneflow/F/__init__.py")).touch()
    # TODO: remove these two lines later
    Path(os.path.join(OUT_PATH, "oneflow/experimental/F")).mkdir(exist_ok=True)
    Path(os.path.join(OUT_PATH, "oneflow/experimental/F/__init__.py")).touch()
    # step 1: extract all exports
    # step 2: merge exports into src in python/
    # step 4: finalize __all__, if it is imported by another module or wrapped in 'oneflow.export', it should appears in __all__
    # step 5: save file and post process (sort imports, format, etc)
    extra_arg = ""
    if args.verbose == False:
        extra_arg += "--quiet"
    if args.autoflake:
        print("[postprocess]", "autoflake")
        subprocess.check_call(
            f"{sys.executable} -m autoflake --in-place --remove-all-unused-imports --exclude '**/*.ast.py' --recursive .",
            shell=True,
            cwd=args.out_dir,
        )
    if args.isort:
        print("[postprocess]", "isort")
        subprocess.check_call(
            f"{sys.executable} -m isort . {extra_arg}", shell=True, cwd=args.out_dir,
        )
    if args.black:
        print("[postprocess]", "black")
        subprocess.check_call(
            f"{sys.executable} -m black . {extra_arg}", shell=True, cwd=args.out_dir,
        )
