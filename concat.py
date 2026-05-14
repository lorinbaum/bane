import os
from pathlib import Path

def collect_folder(p:Path) -> list[Path]:
    return [p / f for f in os.listdir(p) if f.endswith((".h", ".c", "CMakeLists.txt"))]

if __name__ == "__main__":
    folder = Path(__file__).parent

    files:list[Path] = []
    files.append(folder / "CMakeLists.txt")
    files.extend(collect_folder(folder / "src"))
    files.extend(collect_folder(folder / "test"))
    
    concatenation = []
    for file in files:
        with open(file, "r") as f:
            concatenation.append(f"""```{"C" if file.name.endswith((".c", ".h")) else ""} {file.relative_to(folder).as_posix()}
{f.read()}
```
""") 
            
    with open("concatenation.md", "w") as f:
        f.write("Included files:\n")
        for file in files: f.write(f"- {file.relative_to(folder).as_posix()}\n")
        f.write("\n")
        f.write("".join(concatenation))
