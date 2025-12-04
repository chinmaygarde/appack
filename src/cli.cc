#define ARGS_NOEXCEPT

#include <args.hxx>
#include <cstdlib>
#include "package.h"

namespace pack {

static bool PackageAddPaths(std::filesystem::path package_path,
                            std::vector<std::string> path_strings) {
  Package package(package_path);
  if (!package.IsValid()) {
    return false;
  }
  std::vector<std::filesystem::path> paths;
  for (const auto& path_string : path_strings) {
    paths.emplace_back(path_string);
  }
  return package.RegisterPaths(paths);
}

static bool ListFiles(std::filesystem::path package_path) {
  Package package(package_path);
  if (!package.IsValid()) {
    return false;
  }
  auto files = package.ListFiles();
  if (!files.has_value()) {
    return false;
  }
  for (const auto& file : files.value()) {
    std::cout << file.second << " " << file.first << std::endl;
  }
  return true;
}

bool Main(int argc, char const* argv[]) {
  auto appack = args::ArgumentParser{"Manage packages."};

  auto help =
      args::HelpFlag{appack, "help", "Print this help string.", {'h', "help"}};

  // Adding a package.
  auto add =
      args::Command{appack, "add", "Add files or directories to the package."};
  auto package_for_add = args::ValueFlag<std::string>{
      add,
      "package",
      "The package to add files and directories to.",
      {'p', "package"}};
  auto files_and_dirs_to_add = args::PositionalList<std::string>{
      add, "files_and_dirs", "Files and directories to add to the package."};

  // List files.
  auto list = args::Command{appack, "list", "List files in this package."};
  auto package_for_list = args::ValueFlag<std::string>{
      list, "package", "The package to list the files of.", {'p', "package"}};

  if (!appack.ParseCLI(argc, argv)) {
    std::cerr << "Could not parse argument." << std::endl;
    std::cerr << appack.GetErrorMsg() << std::endl;
    std::cerr << appack.Help() << std::endl;
    return false;
  }
  if (add && package_for_add && files_and_dirs_to_add) {
    if (!PackageAddPaths(package_for_add.Get(), files_and_dirs_to_add.Get())) {
      std::cerr << "Could not add paths.";
      return false;
    }
    return true;
  }
  if (list && package_for_list) {
    if (!ListFiles(package_for_list.Get())) {
      std::cerr << "Could not list files.";
      return false;
    }
    return true;
  }
  std::cerr << appack.Help() << std::endl;
  return false;
}

}  // namespace pack

int main(int argc, char const* argv[]) {
  return pack::Main(argc, argv) ? EXIT_SUCCESS : EXIT_FAILURE;
}
