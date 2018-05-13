/* -------------------------------------------------------------------------- *
 *                            OpenSim:  testTRCFileAdapter.cpp                *
 * -------------------------------------------------------------------------- *
 * The OpenSim API is a toolkit for musculoskeletal modeling and simulation.  *
 * See http://opensim.stanford.edu and the NOTICE file for more information.  *
 * OpenSim is developed at Stanford University and supported by the US        *
 * National Institutes of Health (U54 GM072970, R24 HD065690) and by DARPA    *
 * through the Warrior Web program.                                           *
 *                                                                            *
 * Copyright (c) 2005-2017 Stanford University and the Authors                *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may    *
 * not use this file except in compliance with the License. You may obtain a  *
 * copy of the License at http://www.apache.org/licenses/LICENSE-2.0.         *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 * -------------------------------------------------------------------------- */

#include "OpenSim/Common/Adapters.h"
#include <OpenSim/Common/IO.h>

#include <fstream>
#include <cstdio>

void testFailed(const std::string& filename,
                const std::string& origtoken,
                const std::string& copiedtoken) {
    using namespace OpenSim;

    throw Exception{"Test failed: Original and copied TRC files do not match. "
            "Filename = '" + filename + "'. "
            "Expected token = " + origtoken + ". "
            "Copied token = " + copiedtoken + "."};
}

void compareFiles(const std::string& filenameA, 
                  const std::string& filenameB) {
    // Delimiters include carriage return and newline.
    const std::string delims{"\t\r\n"};

    auto eraseEmptyElements = [](std::vector<std::string>& list) {
        std::vector<std::string>::iterator it = list.begin();
        while (it != list.end()) {
            if (it->empty())
                it = list.erase(it);
            else
                ++it;
        }
    };

    std::ifstream fileA{filenameA};
    std::ifstream fileB{filenameB};

    // Use a STOFileAdapter just to access the common
    // FileAdapter utilities.
    OpenSim::STOFileAdapter sfa;
    int lcnt = 0;

    while (fileA && fileB) {
        auto tokensA = sfa.getNextLine(fileA, delims);
        auto tokensB = sfa.getNextLine(fileB, delims);

        ++lcnt;

        if (tokensA.size() != tokensB.size()) {
            // original could have any number of tabs and spaces
            // that are no longer allowed. So ignore them.
            eraseEmptyElements(tokensA);
            eraseEmptyElements(tokensB);
        }

        if (tokensA.size() != tokensB.size()) {
            //if a blank row, skip it
            if (tokensA.empty()) {
                tokensA = sfa.getNextLine(fileA, delims);
                ++lcnt;
            }
            else if (tokensB.empty()) {
                tokensB = sfa.getNextLine(fileB, delims);
                ++lcnt;
            }
        }

        std::string msg{ "Number of elements at line " +
            std::to_string(lcnt) + "did not match." };

        OPENSIM_THROW_IF(tokensA.size() != tokensB.size(),
            OpenSim::Exception, msg);

        for (size_t i = 0; i < tokensA.size(); ++i) {
            if (tokensA[i] == tokensB[i]) {
                continue;
            }
            std::string tokenA{ tokensA[i] };
            std::string tokenB{ tokensB[i] };
            OpenSim::IO::TrimWhitespace(tokenA);
            OpenSim::IO::TrimWhitespace(tokenB);
            tokenA = OpenSim::IO::Lowercase(tokenA);
            tokenB = OpenSim::IO::Lowercase(tokenB);
            if (tokenA == tokenB) {
                continue;
            }
            // We interpreted blank as NaN now revert
            // to compare to the original file with blanks
            if (tokenB == "nan") {
                tokenB = "";
            }
            else if (tokenA == "nan") {
                tokenA = "";
            }
            if (tokenA == tokenB) {
                continue;
            }
            double d_tokenA{};
            double d_tokenB{};
            try {
                d_tokenA = std::stod(tokenA);
                d_tokenB = std::stod(tokenB);
            }
            catch (std::invalid_argument&) {
                testFailed(filenameA, tokenA, tokenB);
            }
            if ((d_tokenA != d_tokenB) &&
                !(std::isnan(d_tokenA) &&
                    std::isnan(d_tokenB))) {
                testFailed(filenameA, tokenA, tokenB);
            }
        } // end for
    } // end while
}

int main() {
    using namespace OpenSim;

    std::vector<std::string> filenames{};
    filenames.push_back("dataWithEformat.trc");
    filenames.push_back("dataWithNaNsOfDifferentCases.trc");
    filenames.push_back("dataWithNaNsWithSpaces.trc");
    filenames.push_back("dataWithBlanksForMissingMarkers.trc");
    // TRCs that are shared with other tests
    filenames.push_back("subject01_synthetic_marker_data.trc");
    filenames.push_back("constraintTest.trc");
    filenames.push_back("subject01_static.trc");
    filenames.push_back("gait10dof18musc_walk_CRLF_line_ending.trc");

    std::string tmpfile{"testtrcfileadapter.trc"};

    bool failed = false;

    std::cout << "Testing TRCFileAdapter::read() and TRCFileAdapter::write()"
              << std::endl;
    for(const auto& filename : filenames) {
        std::cout << "  " << filename << std::endl;
        TRCFileAdapter trcfileadapter{};
        try {
            auto table = trcfileadapter.read(filename);
            trcfileadapter.write(table, tmpfile);
            compareFiles(filename, tmpfile);
        }
        catch (std::exception& ex) {
            std::cout << "Failed because: '" << ex.what() << "'." << std::endl;
            failed = true;
        }
    }

    std::cout << "Testing FileAdapter::readFile() and FileAdapter::writeFile()"
              << std::endl;
    for(const auto& filename : filenames) {
        std::cout << "  " << filename << std::endl;
        try {
            auto table = FileAdapter::readFile(filename).at("markers");
            DataAdapter::InputTables tables{};
            tables.emplace(std::string{ "markers" }, table.get());
            FileAdapter::writeFile(tables, tmpfile);
            compareFiles(filename, tmpfile);
        }
        catch (std::exception& ex) {
            std::cout << "Failed because: '" << ex.what() << "'." << std::endl;
            failed = true;
        }
    }

    std::cout << "Testing TimeSeriesTableVec3 and TRCFileAdapter::write()"
              << std::endl;
    for(const auto& filename : filenames) {
        try {
            std::cout << "  " << filename << std::endl;
            TimeSeriesTableVec3 table{filename};
            TRCFileAdapter::write(table, tmpfile);
            compareFiles(filename, tmpfile);
        }
        catch (std::exception& ex) {
            std::cout << "Failed because: '" << ex.what() << "'." << std::endl;
            failed = true;
        }
    }

    if (failed)
        return 1;

    std::remove(tmpfile.c_str());
    std::cout << "\nAll tests passed!" << std::endl;

    return 0;
}
