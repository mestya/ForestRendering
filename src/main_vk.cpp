#include "vulkan/App.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>

int main()
{
	try {
		forest::vk::App app;
		app.run();
		return EXIT_SUCCESS;
	} catch (std::exception const& e) {
		std::cerr << "Fatal error: " << e.what() << "\n";
		return EXIT_FAILURE;
	}
}

