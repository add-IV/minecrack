#include <string>
#include <set>
#include <regex>
#include <iostream>
#include <algorithm>
#include <boost/program_options.hpp>
#include "JavaRandom.hpp"
#include "minecrack-48bit.hpp"

using namespace std;

/*
 * Get exceptions instead of asserts from boost.
 */
namespace boost {
void assertion_failed(const char* expr, const char* function, const char* file, long line) {
	throw logic_error("Boost assert failed: "s + expr + ", at " + file + ":" + to_string(line) + " in " + function);
}

void assertion_failed_msg(const char* expr, const char* msg, const char* function, const char* file, long line) {
	throw logic_error(
			"Boost assert failed ("s + msg + "): " + "" + expr + ", at " + file + ":" + to_string(line) + " in " +
			function);
}
}

namespace cmdline {

bool verbose;
std::vector<int64_t> chunk_seed_offset;

}

int main(int argc, char** argv) try {
	{
		using namespace boost::program_options;
		options_description options;
		options.add_options()
				("verbose,v", "print extra informations")
				("force,f", "do not bail out if too few chunks are provided");
		if (argc == 1) {
			cerr << "Usage: " << argv[0] << " [options] chunk1X:chunk1Z chunk2X:chunk2Z ..." << endl << "Options:"
			     << endl << options;
			return 0;
		}
		options_description options_with_chunks;
		options_with_chunks.add(options).add_options()("slime-chunk", value<vector<string>>());
		positional_options_description positional;
		positional.add("slime-chunk", -1);
		variables_map vm;
		try {
			store(command_line_parser(argc, argv).options(options_with_chunks).positional(positional).run(),
					vm);
			notify(vm);
		} catch (const boost::program_options::error& e) { throw invalid_argument(e.what()); }
		cmdline::verbose = vm.count("verbose");
		if (!vm.count("slime-chunk"))
			throw invalid_argument("You must specify some slime chunks");
		auto slimechunks_vector = vm["slime-chunk"].as<vector<string>>();
		set<string> slimechunks(slimechunks_vector.begin(), slimechunks_vector.end());
		regex slimechunk_regex("([\\-\\+]?\\d+)\\:([\\-\\+]?\\d+)", regex::ECMAScript | regex::optimize);
		smatch result;
		for (auto& chunkspec: slimechunks) {
			if (!regex_match(chunkspec, result, slimechunk_regex))
				throw invalid_argument("slime chunk coordinate " + chunkspec + " cannot be parsed");
			try {
				int chunkX = stoi(result[1]), chunkZ = stoi(result[2]);
				cmdline::chunk_seed_offset.push_back(slime_seed_offset(chunkX, chunkZ));
			} catch (const out_of_range& o) {
				throw invalid_argument("slime chunk coordinate " + chunkspec + " cannot is out of int range");
			}
		}
		/*
		 * each slime chunk gives log2(10) bits of information on the 48bit seed of the world. At least 15 slime
		 * chunks must be found to have one or just few 48bit candidates.
		 */
		if (cmdline::chunk_seed_offset.size() < 13 && !vm.count("force"))
			throw invalid_argument("Too few slime chunks provided, at least 13 are needed (or use the -f option)");
		if (cmdline::chunk_seed_offset.size() < 15)
			cerr << "Warning: 15 or more slime chunks should be provided" << endl;
	}

	//calculate lower 18bits candidates
	vector<uint32_t> lowbits_candidates = ::lowbits_candidates();
	if (lowbits_candidates.empty())
		throw invalid_argument("The provided slime chunks are wrong or they are generated by a modded Minecraft version");
	if (cmdline::verbose) {
		cerr << "Found " << lowbits_candidates.size() << " lowbits candidates:";
		for (auto c: lowbits_candidates) cerr << ' ' << c;
		cerr << endl;
	}

	auto seeds = test_seeds(lowbits_candidates);
	sort(seeds.begin(), seeds.end());
	for (auto s: seeds) cout << s << endl;

} catch (const invalid_argument& e) {
	cerr << "Invalid argument: " << e.what() << endl;
	return 1;
} catch (const exception& e) {
	cerr << e.what() << endl;
	return 2;
}
