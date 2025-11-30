#define ARGS_NOEXCEPT

#include <args.hxx>
#include <cstdlib>

namespace appack {

bool Main(int argc, char const* argv[]) {
  auto parser = args::ArgumentParser{"Manage packages."};
  args::HelpFlag help(parser, "help", "Display help menu", {'h', "help"});
  if (!parser.ParseCLI(argc, argv)) {
    std::cerr << "Could not parse arguments." << std::endl
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
