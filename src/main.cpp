#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <vector>

#include <getopt.h>

#include "boyer_moore.h"
#include "brute_force.h"
#include "kmp.h"
#include "sellers.h"
#include "ukkonen.h"
#include "utils.h"

enum class StringMatcher {
  kBoyerMoore,
  kDefault,
  kKnuthMorrisPratt,
  kSellers,
  kUkkonen
};

void PrintHelpFunction() {
  std::cout << "Usage: pmt [options] pattern textfile [textfile ...]\n\n  \"pattern\" is the "
      << "word you want to find in the specified text.\n  \"textfile\" is the file with the text "
      << "to be processed (wildcards are also supported to choose multiple files, but you need to "
      << "specify the argument under quotes due to the parsing of the options).\n\nAvailable option"
      << "s:\n    -a --algorithm\tSpecifies a string matching algorithm supported by this\n\t\t\t"
      << "tool. Valid arguments are: \"bm\" (Boyer-Moore), \"kmp\"\n\t\t\t(Knuth-Morris-Pratt), "
      << "\"sel" "\" (Sellers) and \"ukk\"\n\t\t\t(Ukkonen). If Sellers or Ukkonen are chosen, "
      << "option -e\n\t\t\tor --edit must be enabled.\n\n    " << std::setw(14) << std::left 
      << "-c --count" << "\tPrint only the number of occurrences of the specified\n\t\t\tpattern "
      << "on the text.\n\n    " << std::setw(14) << std::left << "-e --edit" << "\tMaximum edit "
      << "distance. Used for approximate string\n\t\t\tmatching.\n\n    " << std::setw(14)
      << std::left << "-p --pattern" << "\tSpecifies a file containing all the patterns which we "
      << "\n\t\t\twant to find on the text. If this option is enabled,\n\t\t\t\"pattern\" will be "
      << "interpreted as a text file.\n" << std::endl;

}

int main(int argc, char *argv[]) {
  option long_options[] = {
    // Additional option if the user wants to know the usage of this tool.
    {"help", no_argument, nullptr, 'h'},
    // Options from project specification.
    {"algorithm", required_argument, nullptr, 'a'},
    {"count", no_argument, nullptr, 'c'},
    {"edit", required_argument, nullptr, 'e'},
    {"pattern", no_argument, nullptr, 'p'}
  };

  int c = 0;
  int option_index = 0;

  StringMatcher algorithm = StringMatcher::kDefault;
  int max_edit_distance = -1;
  bool num_occurrences_only = false;
  bool read_pattern_files = false;
  std::string option_arg;
  size_t sz;

  // #######################
  // ### Process options ###
  // #######################
  while ((c = getopt_long(argc, argv, "a:ce:hp", long_options, &option_index)) != -1) {
    switch (c) {
      case 'a':
        option_arg = optarg;
        
        if (!option_arg.compare("bm")) {
          algorithm = StringMatcher::kBoyerMoore;
        } else if (!option_arg.compare("kmp")) {
          algorithm = StringMatcher::kKnuthMorrisPratt;
        } else if (!option_arg.compare("sel")) {
          algorithm = StringMatcher::kSellers;
        } else if (!option_arg.compare("ukk")) {
          algorithm = StringMatcher::kUkkonen;
        } else {
          std::cout << "Program exited with code 1: inexistent algorithm." << std::endl;
          return 1;
        }

        break;

      case 'c':
        num_occurrences_only = true;
        break;

      case 'e':
        try {
          option_arg = optarg;
          max_edit_distance = std::stoi(option_arg, &sz);

          if (sz != option_arg.size() || max_edit_distance < 0) {
            std::cout << "Program exited with code -2: invalid edit distance (must be a "
                << "non-negative integer)." << std::endl;
            return -2;
          }
        } catch (std::invalid_argument &e) {
          std::cout << e.what() << std::endl;
          std::cout << "Program exited with code -2: invalid edit distance (must be a positive "
              << "integer)." << std::endl;
          return -2;
        } catch (std::out_of_range &e) {
          std::cout << e.what() << std::endl;
          std::cout << "Program exited with code -3." << std::endl;
          return -3;
        }

        break;

      case 'h':
        PrintHelpFunction();
        return 0;

      case 'p':
        read_pattern_files = true;
        break;

      default:
        std::cout << "Program exited with code -1: inexistent option." << std::endl;
        return -1;
    }
  }

  // ####################################
  // ### Perform remaining validation ###
  // ####################################
  if (max_edit_distance >= 0 && (algorithm == StringMatcher::kBoyerMoore ||
      algorithm == StringMatcher::kKnuthMorrisPratt)) {
    std::cout << "Program exited with code 2: cannot use KMP or Boyer-Moore algorithms on "
        << "approximate string matching. Disable option -e or --edit to perform exact string "
        << "matching." << std::endl;
    return 2;
  } else if (max_edit_distance < 0 && (algorithm == StringMatcher::kSellers ||
             algorithm == StringMatcher::kUkkonen)) {
    std::cout << "Program exited with code 3: cannot use Sellers or Ukkonen algorithms on exact "
        << "string matching. Enable option -e or --edit to perform approximate string matching."
        << std::endl;
    return 3;
  }

  if (optind >= argc) {
    std::cout << "Program exited with code 4: incorrect number of arguments (type pmt --help for "
        << "more details)." << std::endl;
    return 4;
  }

  // ####################################
  // ### Read patterns from arguments ###
  // ####################################
  std::vector<std::string> patterns;
  if (!read_pattern_files) {
    patterns.push_back(argv[optind++]);

    if (optind >= argc) {
      std::cout << "Program exited with code 4: incorrect number of arguments (type pmt --help for "
          << "more details)." << std::endl;
      return 4;
    }
  } else {
    std::vector<std::string> pattern_files = pmt::GetFilenames(argv[optind++]);
    for (size_t i = 0; i < pattern_files.size(); ++i) {
      std::ifstream ifs(pattern_files[i], std::ifstream::binary);
      if (!ifs) {
        std::cout << "Program exited with code 5: cannot open file(s) from argument list."
            << std::endl;
        return 5;
      }

      std::string line;      
      while (std::getline(ifs, line)) {
        patterns.push_back(line);
      }

      ifs.close();
    }
  }

  // ####################################################
  // ### For each file, find matches for all patterns ###
  // ####################################################
  bool has_multiple_text_files = (argc - optind) > 1; 
  const int kMaxSize = 1 << 23;  // 8 MB.
  std::vector<char> buffer(kMaxSize);
  
  pmt::KMPStringMatcher kmp;
  pmt::BoyerMooreStringMatcher bm;

  for (int i = optind; i < argc; ++i) {
    std::vector<std::string> text_files = pmt::GetFilenames(argv[i]);
    has_multiple_text_files |= text_files.size() > 1;

    for (size_t tf = 0; tf < text_files.size(); ++tf) {
      std::ifstream ifs(text_files[tf], std::ifstream::binary);
      if (!ifs) {
        std::cout << "Program exited with code 5: cannot open file(s) from argument list."
            << std::endl;
        return 5;
      }

      size_t total = 0;
      size_t pos = 0;

      while (!ifs.eof()) {
        std::string line;
        int size = 0;

        while (std::getline(ifs, line)) {
          line += "\n";
          int tmp = size + static_cast<int>(line.size());
          if (tmp < kMaxSize) {
            std::copy(line.begin(), line.end(), buffer.begin() + size);
	          size = tmp;
          } else {
            ifs.clear();
            ifs.seekg(pos);
            break;
          }
          
          pos = ifs.tellg();
        }
        
        for (size_t j = 0; j < patterns.size(); ++j) {
          std::vector<int> occurrences;
          if (max_edit_distance < 0) {  // Exact string matching.
            switch (algorithm) {
              case StringMatcher::kBoyerMoore:
                bm.SetPattern(patterns[j]);
                occurrences = bm.FindOccurrences(buffer.data());
                break;

              case StringMatcher::kKnuthMorrisPratt:
                kmp.SetPattern(patterns[j]);
                occurrences = kmp.FindOccurrences(buffer.data());
                break;

              default:  // The default approach of this tool for exact string matching.
                if (patterns[j].size() <= 1) {
                  occurrences = pmt::BruteForceStringMatcher(patterns[j], buffer.data());
                } else {
                  // Since alphabet is already set to be ASCII in this application, we use the
                  // Boyer-Moore algorithm as default.
                  bm.SetPattern(patterns[j]);
                  occurrences = bm.FindOccurrences(buffer.data());
                }

                break;
            }
          } else {  // Approximate string matching.
            switch (algorithm) {
              case StringMatcher::kSellers:
                occurrences = pmt::SellersStringMatcher(patterns[j], buffer.data(),
                                                        max_edit_distance);
                break;

              case StringMatcher::kUkkonen:
                occurrences = pmt::UkkonenStringMatcher(patterns[j], buffer.data(),
                                                        max_edit_distance);
                break;

              default:  // The default approach of this tool for approximate string matching.
                occurrences = pmt::SellersStringMatcher(patterns[j], buffer.data(),
                                                        max_edit_distance);
                break;
            }

            std::transform(occurrences.begin(), occurrences.end(), occurrences.begin(),
                           [&patterns, j] (int val) -> int { 
                              return val + 1 - patterns[j].size() > 0 ?
                                     val + 1 - patterns[j].size() : 0;
                           });
          }

          if (!num_occurrences_only) {
            std::cout << pmt::PrintOccurrences(occurrences, buffer.data(),
                                               static_cast<int>(patterns[j].size()));
            std::cout << std::endl;
          }

          total += occurrences.size();
        }
        
        std::fill(buffer.begin(), buffer.end(), '\0');
      }

      if (has_multiple_text_files && num_occurrences_only) {
        std::cout << text_files[tf] << ":" << total << std::endl;
      } else if (num_occurrences_only) {
        std::cout << total << std::endl;
      }
    }
  }

  return 0;
}
