// Command-line entry point for listing a GFS archive as TSV.

#include "gfs.hpp"

#include <exception>
#include <iostream>

#ifdef _WIN32
int wmain(int argc, wchar_t** argv)
#else
int main(int argc, char** argv)
#endif
{
    if (argc != 2) {
        std::cerr << "Usage: skullgirls-gfs <archive.gfs>\n";
        return 2;
    }

    try {
        const auto entries = skullgirls::read_archive_index(argv[1]);
        std::cout << "offset\tsize\talignment\tname\n";
        for (const auto& entry : entries) {
            std::cout << entry.offset << '\t' << entry.size << '\t'
                      << entry.alignment << '\t' << entry.name << '\n';
        }
        return std::cout ? 0 : 1;
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
