#define ARGS_NOEXCEPT

#include <args.hxx>
#include <cstdlib>

namespace appack {

bool Main(int argc, char const* argv[]) {
  auto parser = args::ArgumentParser{"Manage packages."};
  args::HelpFlag help(parser, "help", "Display help menu", {'h', "help"});

  // Work on assets.
  args::Command assets(parser, "assets", "Manage assets in the package.");
  args::Command add(assets, "add", "Add files and directories to the package.");
  args::Command list(assets, "list", "List the assets in the package.");
  args::Command clear(assets, "clear", "Clear all assets in the package.");

  // Installation.
  args::Command install(parser, "install", "Install the package.");

  if (!parser.ParseCLI(argc, argv)) {
    std::cerr << "Invalid arguments." << std::endl
              << parser.Help() << std::endl;
    return false;
  }
  if (help) {
    std::cout << parser.Help() << std::endl;
    return true;
  }
  return true;
}

}  // namespace appack

int main(int argc, char const* argv[]) {
  return appack::Main(argc, argv) ? EXIT_SUCCESS : EXIT_FAILURE;
}
