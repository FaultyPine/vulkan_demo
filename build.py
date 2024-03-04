import os, sys, time
PYTHON_SCRIPT_PATH = os.path.realpath(os.path.dirname(__file__))



APP_NAME = "VulkanDemo"
SOURCE_DIR = "src"
RESOURCE_DIRECTORY = f"{PYTHON_SCRIPT_PATH}/res/"
BUILD_LIB_DIR = f"{PYTHON_SCRIPT_PATH}/build"
BIN_DIR = f"{PYTHON_SCRIPT_PATH}/bin/"


# since we don't have a proper pythonic package folder structure, choosing to import our build utils this way
sys.path.append(f"{PYTHON_SCRIPT_PATH}/build_tools")
from build_utils import *

EXE_NAME = f"{APP_NAME}.exe" if is_windows() else f"{APP_NAME}.out"


#-------------------------------------------------------------------

def get_compiler_args_clang():
    # -g<level>   0/1/2/full  debug level
    # -O<level> optimization level
    # -shared   build dll
    # -D <name>  preprocessor define
    # -c    only compile, don't link (provided by default)
    # -o    output object files (provided by default)
    # NOTE: the -D_DLL flag seems be the (a?) key to dynamically linking CRT. Maybe some windows header has a gaurd on a pragma link?

    to_root = "."
    include_root_paths = ["", "external", "src"]
    include_paths = include_paths_str(to_root, include_root_paths)
    return clean_string(f"""
        -g -O0 -std=c++17 {build_common_compiler_args()} {include_paths} {cpp_ver_arg()}
    """)


def get_linker_args_lld_link():
    to_root = "."
    library_root_paths = ["", "external"]
    library_paths = library_paths_str(to_root, library_root_paths)
    return clean_string(f"""
        {library_paths}
        /OUT:{BUILD_LIB_DIR}/{EXE_NAME}
        /DEBUG
        glfw/lib/glfw3_mt.lib vulkan_lib/vulkan-1.lib
        libcmt.lib user32.lib gdi32.lib shell32.lib
    """)

def get_game_sources():
    return get_files_with_ext_recursive_walk(SOURCE_DIR, "cpp")

def build_game(standalone_ninjafile_dir = ""):
    generic_ninja_build(PYTHON_SCRIPT_PATH, get_compiler_args_clang(), get_linker_args_lld_link(), BUILD_LIB_DIR, get_game_sources, EXE_NAME, BIN_DIR)
   
def run_game():
    if is_windows():
        command(f"\"{BIN_DIR}\\{EXE_NAME}\" {RESOURCE_DIRECTORY}")

def main():
    args = sys.argv[1:]
    standalone_ninjafile_dir = os.path.join(PYTHON_SCRIPT_PATH, BIN_DIR)
    if len(args) > 0:
        if "clean" in args:
            clean(BUILD_LIB_DIR)
            clean(BIN_DIR)
        elif "regen" in args:
            generate_ninjafile(PYTHON_SCRIPT_PATH, get_compiler_args_clang(), get_linker_args_lld_link(), BUILD_LIB_DIR, get_game_sources, EXE_NAME, True)
        elif "norun" in args:
            build_game(standalone_ninjafile_dir)
        elif "run" in args:
            run_game()
        elif "help" in args:
            print("TODO: implement help text")
        else:
            print("Unknown argument passed to game build script!")
    else:
        # default no arg behavior
        build_game(standalone_ninjafile_dir)


if __name__ == "__main__":
    main()