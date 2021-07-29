# python3 -m pip install astpretty
# requires python3.9 to run
import ast
import os
import argparse
import ast
import subprocess
import multiprocessing
from pathlib import Path
import astpretty
import sys
from astpretty import pprint
from collections import Counter

from typed_ast.ast27 import alias

parser = argparse.ArgumentParser()
parser.add_argument(
    "--out_dir", type=str, default="python",
)
parser.add_argument("--verbose", "-v", action="store_true")
parser.add_argument("--ast", action="store_true")
ONEFLOW_TEST_PYTORCH_VISION_DIR = os.getenv("ONEFLOW_TEST_PYTORCH_VISION_DIR")
parser.add_argument(
    "--pytorch_vision_dir", type=str, default=ONEFLOW_TEST_PYTORCH_VISION_DIR,
)
args = parser.parse_args()

ONEFLOW_TEST_PYTORCH_VISION_PATH = Path(args.pytorch_vision_dir)
SHOULD_SAVE_ASTSHOULD_SAVE_AST = args.ast


class CompatibilityVisitor(ast.NodeVisitor):
    from ast import ImportFrom, Import

    def __init__(self) -> None:
        super().__init__()
        self.module_num = Counter()
        self.attribute_num = Counter()

    def visit_ImportFrom(self, node: ImportFrom):
        if node.module:
            if node.module == "torch" or "torch." in node.module:
                for a in node.names:
                    assert isinstance(a, ast.alias)
                    pprint(a)
                self.module_num.update([node.module])

    def visit_Import(self, node: Import):
        modules = [
            a.name
            for a in node.names
            if a.name.startswith("torch.") and a.name != "torch"
        ]
        self.module_num.update(modules)


def analyze_py(args):
    src: Path = args["src"]
    tree = ast.parse(src.read_text())
    v = CompatibilityVisitor()
    v.visit(tree)
    if SHOULD_SAVE_ASTSHOULD_SAVE_AST:
        src.with_suffix(".ast.py").write_text(
            f"""from ast import *
{astpretty.pformat(tree)}
"""
        )
    return v


if __name__ == "__main__":
    print(ONEFLOW_TEST_PYTORCH_VISION_PATH)
    py_srcs = ONEFLOW_TEST_PYTORCH_VISION_PATH.glob("**/*.py")
    pool = multiprocessing.Pool()
    results = pool.map(analyze_py, [{"src": src,} for src in py_srcs],)
    pool.close()
    module_num = Counter()
    for r in results:
        module_num.update(r.module_num)
    print(module_num.most_common())
