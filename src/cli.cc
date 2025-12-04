#define ARGS_NOEXCEPT

#include <args.hxx>
#include <cstdlib>

namespace appack {

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

  if (!appack.ParseCLI(argc, argv)) {
    std::cerr << "Could not parse argument." << std::endl;
    std::cerr << appack.GetErrorMsg() << std::endl;
    std::cerr << appack.Help() << std::endl;
    return false;
  }
  if (add && package_for_add && files_and_dirs_to_add) {
    std::cout << "Adding packaging.";
    return true;
  }
  std::cerr << appack.Help() << std::endl;
  return false;
}

}  // namespace appack

int main(int argc, char const* argv[]) {
  return appack::Main(argc, argv) ? EXIT_SUCCESS : EXIT_FAILURE;
}
