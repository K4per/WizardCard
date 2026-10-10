"""Check the native module DAG and projection headers used by AI/presentation."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

def main():
    cmake = (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8-sig')
    graph = {}
    for target, args in re.findall(r'target_link_libraries\((wizard_\w+)\s+([^)]*)\)', cmake):
        graph.setdefault(target, set()).update(re.findall(r'\bwizard_\w+\b', args))
    visited, stack = set(), []
    def visit(node):
        if node in stack:
            raise ValueError('Module cycle: ' + ' -> '.join(stack + [node]))
        if node in visited:
            return
        stack.append(node)
        for dependency in graph.get(node, ()):
            visit(dependency)
        stack.pop()
        visited.add(node)
    for target in graph:
        visit(target)
    def closure(node):
        result = {node}
        for dependency in graph.get(node, ()):
            result.update(closure(dependency))
        return result
    if 'wizard_network' in closure('wizard_application'):
        raise ValueError('Application must receive its network adapter from the composition root')
    def headers(path, seen=None):
        seen = set() if seen is None else seen
        if path in seen:
            return seen
        seen.add(path)
        text = path.read_text(encoding='utf-8-sig')
        if re.search(r'\b(struct GameState|class GameEngine)\b', text):
            raise ValueError(f'Projected consumer exposes engine internals via {path.relative_to(ROOT)}')
        for include in re.findall(r'#include "(wizard/[^\"]+)"', text):
            headers(ROOT / 'include' / include, seen)
        return seen
    for name in ('ai', 'interaction', 'presentation', 'sound', 'session'):
        headers(ROOT / 'include/wizard' / (name + '.hpp'))
    print(f'{len(visited)} targets form an acyclic graph; application is transport-independent; projection headers hide GameState/GameEngine')

if __name__ == '__main__':
    main()
