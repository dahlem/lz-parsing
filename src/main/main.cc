// Copyright (C) 2013, 2015 Dominik Dahlem <Dominik.Dahlem@gmail.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

#if HAVE_CONFIG_H
# include <config.h>
#endif

#ifndef NDEBUG
# include <cassert>
# include <boost/assert.hpp>
#endif /* NDEBUG */

#include <algorithm>
#include <iterator>
#include <fstream>
#include <iostream>
#include <vector>
#include <map>

#ifndef __STDC_CONSTANT_MACROS
# define __STDC_CONSTANT_MACROS
#endif /* __STDC_CONSTANT_MACROS */

#include <boost/cstdint.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>

#include "CL.hh"
namespace cmain = lz::main;

typedef std::vector<std::string> SEQUENCE;
typedef std::vector<std::pair<boost::uint32_t, SEQUENCE> > SEQUENCES;
typedef std::pair<boost::uint32_t, boost::uint32_t> VAL;
typedef std::map<std::string, VAL > DICT;


/**
 *
 */
void prune(DICT &h)
{
  for(DICT::iterator it = h.begin(), it_end = h.end(); it != it_end;)
  {
    if ((it->second).second == 0) {
      h.erase(it++);
    } else {
      ++it;
    }
  }

  // re-assign the reference
  boost::uint32_t idx = 0;
  for(DICT::iterator it = h.begin(), it_end = h.end(); it != it_end; ++it)
  {
    (it->second).first = idx;
    idx++;
  }
}

/**
 * This function is just used to add a string into the dictionary without encoding
 */
void add(DICT &h, std::string e)
{
  DICT::iterator it;
  boost::uint32_t index = 0;

  it = h.find(e);

  if (it == h.end()) {
    // new group
    index = h.size()+1;
    VAL val = VAL(index, 0);
    h.insert(std::pair<std::string, VAL>(e, val));
  }
}

void lempelZivDictionary_parsing(SEQUENCE &sequence, DICT &shortSeqs, boost::uint32_t i, boost::uint16_t m)
{
  DICT::iterator it_hist = shortSeqs.end();
  SEQUENCE::iterator it_cur = sequence.begin();
  std::advance(it_cur, i);

  SEQUENCE phrase;
  for (; it_cur != sequence.end(); it_cur++) {
    std::string k = *it_cur;
    phrase.push_back(k);

    std::ostringstream phraseStr;
    std::copy(phrase.begin(), phrase.end()-1, std::ostream_iterator<SEQUENCE::value_type>(phraseStr, "<>"));
    phraseStr << phrase.back();

    // check whether current word is in dictionary
    // 1. if yes, continue appending
    // 2. if no, add and reset word
    if (shortSeqs.find(phraseStr.str()) == it_hist) {
      add(shortSeqs, phraseStr.str());

      if (m > 0) {
        boost::int16_t pos = it_cur - sequence.begin();
        boost::int16_t maxShift = m + 1;
        boost::int16_t backShift = std::min(pos, maxShift);
        std::advance(it_cur, -backShift);
      }

      phrase.clear();
    }
  }
}

int main(int argc, char *argv[])
{
  cmain::args_t args;
  cmain::CL cl;

  if (cl.parse(argc, argv, args)) {
    return EXIT_SUCCESS;
  }

  std::ifstream sequenceFile;
  sequenceFile.open(args.sequence.c_str());
  if (!sequenceFile.is_open()) {
    std::cerr << "Could not open file: " << args.sequence << std::endl;
    return EXIT_FAILURE;
  }

  SEQUENCE sequence;
  SEQUENCES sequences;
  boost::uint32_t curSequenceID = std::numeric_limits<boost::uint32_t>::max();
  std::string line;
  DICT dict;

  // parsing
  std::cout << "Parsing..." << std::endl;
  while (!sequenceFile.eof()) {
    std::getline(sequenceFile, line);

    if (line != "") {
      std::vector<std::string> splitVec;
      boost::split(splitVec, line, boost::is_any_of(","), boost::token_compress_on);

      if (splitVec.size() != 3) {
        std::cerr << "Expect three elements, but got: " << line << std::endl;
        return EXIT_FAILURE;
      } else {
        boost::uint32_t sequenceID = boost::lexical_cast<boost::uint32_t>(splitVec[0]);

        if (sequenceID != curSequenceID) {
          // parse lz sequence
          SEQUENCE hist(sequence);
          sequences.push_back(std::pair<boost::uint32_t, SEQUENCE> (curSequenceID, hist));
          lempelZivDictionary_parsing(sequence, dict, 0, args.back_shifting);
          if (sequence.size() > 0 && args.input_shifting > 0) {
            boost::uint16_t shift = args.input_shifting;
            if (args.input_shifting >= (sequence.size()-1)) {
              shift = sequence.size()-1;
            }
            for (boost::uint16_t i = 1; i <= shift; ++i) {
              lempelZivDictionary_parsing(sequence, dict, i, args.back_shifting);
            }
          }
          sequence.clear();
          sequence.push_back(splitVec[2]);
          curSequenceID = sequenceID;
        } else {
          sequence.push_back(splitVec[2]);
        }
      }
    }
  }
  sequenceFile.close();

  // parse the last sequence
  SEQUENCE hist(sequence);
  sequences.push_back(std::pair<boost::uint32_t, SEQUENCE> (curSequenceID, hist));
  lempelZivDictionary_parsing(sequence, dict, 0, args.back_shifting);
  if (sequence.size() > 0 && args.input_shifting > 0) {
    boost::uint16_t shift = args.input_shifting;
    if (args.input_shifting >= (sequence.size()-1)) {
      shift = sequence.size()-1;
    }
    for (boost::uint16_t i = 1; i <= shift; ++i) {
      lempelZivDictionary_parsing(sequence, dict, i, args.back_shifting);
    }
  }

  // prune the dictionary
  if (args.prune) {
    std::cout << "Pruning..." << std::endl;
    prune(dict);
  }

  std::cout << "Serialising the dictionary..." << std::endl;
  std::string filename = args.results_dir + "/dictionary.dat";
  std::ofstream outDict(filename.c_str(), std::ios::out);
  BOOST_FOREACH(const DICT::value_type& entry, dict) {
    outDict << entry.first << "," << (entry.second).first << "," << (entry.second).second << std::endl;
  }
  outDict.close();

  return EXIT_SUCCESS;
}
