// Copyright (C) 2013 Dominik Dahlem <Dominik.Dahlem@gmail.com>
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

typedef std::vector<boost::uint32_t> ENCODING;
typedef std::vector<std::pair<boost::uint32_t, ENCODING> > ENCODINGS;
typedef std::vector<std::string> SEQUENCE;
typedef std::vector<std::pair<boost::uint32_t, SEQUENCE> > SEQUENCES;
typedef std::pair<boost::uint32_t, boost::uint32_t> VAL;
typedef std::map<std::string, VAL > DICT;
typedef std::map<boost::uint32_t, boost::uint32_t> TRANSLATE;


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
        (it->second).second = 0;
        idx++;
    }
}

void prune(DICT &h, TRANSLATE &t)
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
    t.clear();
    boost::uint32_t idx = 0;
    for(DICT::iterator it = h.begin(), it_end = h.end(); it != it_end; ++it)
    {
        t[(it->second).first] = idx; // map the old value to a new value
        (it->second).first = idx;
        idx++;
    }
}

void translate(ENCODINGS &e, TRANSLATE &t)
{
    ENCODINGS::iterator es_it = e.begin(), es_end = e.end();
    for (; es_it != es_end; ++es_it) {
        ENCODING::iterator e_it = (es_it->second).begin(), e_end = (es_it->second).end();
        for (; e_it != e_end; ++e_it) {
            *e_it = t[*e_it];
        }
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

/**
 * This function adds a string and returns the index for encoding.
 */
boost::uint32_t encode(DICT &h, std::string e)
{
    DICT::iterator it;
    boost::uint32_t index = 0;

    it = h.find(e);

    if (it != h.end()) {
        // group exists already
        index = (it->second).first;
        (it->second).second++;
    } else {
        // new group
        index = h.size()+1;
        VAL val = VAL(index, 1);
        h.insert(std::pair<std::string, VAL>(e, val));
    }

    return index;
}

void lempelZivDictionary_parsing(std::vector<std::string> &sequence, DICT &shortSeqs, boost::uint32_t i)
{
    DICT::iterator it_hist = shortSeqs.end();
    std::vector<std::string>::iterator it_cur = sequence.begin();
    std::advance(it_cur, i);

    // add alphabet
    for (boost::uint32_t k = i; k < sequence.size(); ++k) {
        add(shortSeqs, sequence[k]);
    }

    std::vector<std::string> w, wk;
    for (; it_cur != sequence.end(); it_cur++) {
        std::string k = *it_cur;

        wk.clear();
        wk.insert(wk.end(), w.begin(), w.end());
        wk.push_back(k);

        std::ostringstream wkStr, wStr;
        std::copy(wk.begin(), wk.end()-1, std::ostream_iterator<std::string>(wkStr, "<>"));
        wkStr << wk.back();

        if (w.size()) {
            std::copy(w.begin(), w.end()-1, std::ostream_iterator<std::string>(wStr, "<>"));
            wStr << w.back();
        }

        if (wk.size() >= 4) {
            encode(shortSeqs, wStr.str());
            w.clear();
            w.push_back(k);
        } else {
            it_hist = shortSeqs.find(wkStr.str());

            if (it_hist != shortSeqs.end()) {
                w = wk;
            } else {
                add(shortSeqs, wkStr.str());
                encode(shortSeqs, wStr.str());
                w.clear();
                w.push_back(k);
            }
        }
    }
    if (w.size() == 1) {
        std::ostringstream wStr;
        wStr << w.back();
        encode(shortSeqs, wStr.str());
    } else if (w.size() > 1) {
        std::ostringstream wStr;
        std::copy(w.begin(), w.end()-1, std::ostream_iterator<std::string>(wStr, "<>"));
        wStr << w.back();
        encode(shortSeqs, wStr.str());
    }
}

void lempelZivDictionary_encoding(std::vector<std::string> &sequence, DICT &shortSeqs, std::vector<boost::uint32_t> &encoding, boost::uint32_t i)
{
    DICT::iterator it_hist = shortSeqs.end();
    boost::uint32_t code = 0;
    std::vector<std::string>::iterator it_cur = sequence.begin();
    std::advance(it_cur, i);

    std::vector<std::string> w, wk;
    for (; it_cur != sequence.end(); it_cur++) {
        std::string k = *it_cur;

        wk.clear();
        wk.insert(wk.end(), w.begin(), w.end());
        wk.push_back(k);

        std::ostringstream wkStr, wStr;
        std::copy(wk.begin(), wk.end()-1, std::ostream_iterator<std::string>(wkStr, "<>"));
        wkStr << wk.back();

        if (w.size()) {
            std::copy(w.begin(), w.end()-1, std::ostream_iterator<std::string>(wStr, "<>"));
            wStr << w.back();
        }

        if (wk.size() >= 4) {
            code = encode(shortSeqs, wStr.str());
            encoding.push_back(code);
            w.clear();
            w.push_back(k);
        } else {
            it_hist = shortSeqs.find(wkStr.str());

            if (it_hist != shortSeqs.end()) {
                w = wk;
            } else {
                code = encode(shortSeqs, wStr.str());
                encoding.push_back(code);
                w.clear();
                w.push_back(k);
            }
        }
    }
    if (w.size() == 1) {
        std::ostringstream wStr;
        wStr << w.back();
        code = encode(shortSeqs, wStr.str());
        encoding.push_back(code);
    } else if (w.size() > 1) {
        std::ostringstream wStr;
        std::copy(w.begin(), w.end()-1, std::ostream_iterator<std::string>(wStr, "<>"));
        wStr << w.back();
        code = encode(shortSeqs, wStr.str());
        encoding.push_back(code);
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
        boost::trim(line);

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
                    lempelZivDictionary_parsing(sequence, dict, 0);
                    if (sequence.size() > 0) {
                        lempelZivDictionary_parsing(sequence, dict, 1);
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
    lempelZivDictionary_parsing(sequence, dict, 0);
    if ((sequence.size() > 0) && (args.twopass_parse)) {
        lempelZivDictionary_parsing(sequence, dict, 1);
    }

    // prune the dictionary
    std::cout << "Pruning..." << std::endl;
    prune(dict);

    // encoding
    std::cout << "Encoding " << sequences.size() << " sequences..." << std::endl;
    SEQUENCES::iterator p_it = sequences.begin(), p_end = sequences.end();
    ENCODINGS encodings;
    for (; p_it != p_end; ++p_it) {
        if ((p_it->second).size() >= args.min) {
            // parse lz sequence
            ENCODING encoding;
            lempelZivDictionary_encoding(p_it->second, dict, encoding, 0);
            if (((p_it->second).size() > 0) && (args.twopass_encode)) {
                lempelZivDictionary_encoding(p_it->second, dict, encoding, 1);
            }
            encodings.push_back(std::pair<boost::uint32_t, ENCODING> (p_it->first, encoding));
        }
    }

    TRANSLATE t;
    prune(dict, t);
    translate(encodings, t);

    std::string filename = args.results_dir + "/encoding.dat";
    std::ofstream outEncoding(filename.c_str(), std::ios::out);
    ENCODINGS::iterator e_it = encodings.begin(), e_end = encodings.end();
    for (; e_it != e_end; ++e_it) {
        outEncoding << e_it->first << ",";
        std::copy((e_it->second).begin(), (e_it->second).end()-1, std::ostream_iterator<boost::uint32_t>(outEncoding, " "));
        outEncoding << (e_it->second).back() << std::endl;
    }
    outEncoding.close();

    std::cout << "Serialising the dictionary..." << std::endl;
    filename = args.results_dir + "/dictionary.dat";
    std::ofstream outDict(filename.c_str(), std::ios::out);
    BOOST_FOREACH(const DICT::value_type& entry, dict) {
        outDict << entry.first << "," << (entry.second).first << "," << (entry.second).second << std::endl;
    }
    outDict.close();

    return EXIT_SUCCESS;
}
