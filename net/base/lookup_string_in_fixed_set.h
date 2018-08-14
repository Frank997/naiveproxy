// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_LOOKUP_STRING_IN_FIXED_SET_H_
#define NET_BASE_LOOKUP_STRING_IN_FIXED_SET_H_

#include <stddef.h>

#include "net/base/net_export.h"

namespace net {

enum {
  kDafsaNotFound = -1,  // key is not in set
  kDafsaFound = 0,      // key is in set
  // The following return values are used by the implementation of
  // GetDomainAndRegistry() and are probably not generally useful.
  kDafsaExceptionRule = 1,  // key excluded from set via exception
  kDafsaWildcardRule = 2,   // key matched a wildcard rule
  kDafsaPrivateRule = 4,    // key matched a private rule
};

// Looks up the string |key| with length |key_length| in a fixed set of
// strings. The set of strings must be known at compile time. It is converted to
// a graph structure named a DAFSA (Deterministic Acyclic Finite State
// Automaton) by the script make_dafsa.py during compilation. This permits
// efficient (in time and space) lookup. The graph generated by make_dafsa.py
// takes the form of a constant byte array which should be supplied via the
// |graph| and |length| parameters.  The return value is kDafsaNotFound,
// kDafsaFound, or a bitmap consisting of one or more of kDafsaExceptionRule,
// kDafsaWildcardRule and kDafsaPrivateRule ORed together.
//
// TODO(nick): Replace this with FixedSetIncrementalLookup everywhere.
NET_EXPORT int LookupStringInFixedSet(const unsigned char* graph,
                                      size_t length,
                                      const char* key,
                                      size_t key_length);

// FixedSetIncrementalLookup provides efficient membership and prefix queries
// against a fixed set of strings. The set of strings must be known at compile
// time. The set is converted to a graph structure named a DAFSA (Deterministic
// Acyclic Finite State Automaton) by the script //net/tools/dafsa/make_dafsa.py
// during compilation. The conversion generates a C++ header file defining the
// encoded graph as a constant byte array. This class provides a fast, constant-
// space lookup operation against such byte arrays.
//
// The lookup proceeds incrementally, with input characters provided one at a
// time. This approach allow queries of the form: "given an input string, which
// prefixes of that string that appear in the fixed set?" As the matching
// prefixes (and their result codes) are enumerated, the most suitable match
// among them can be selected in a single pass.
//
// This class can also be used to perform suffix queries (instead of prefix
// queries) against a fixed set, so long as the DAFSA is constructed on reversed
// values, and the input is provided in reverse order.
//
// Example usage for simple membership query; |input| is a std::string:
//
//    FixedSetIncrementalLookup lookup(kDafsa, sizeof(kDafsa));
//    for (char c : input) {
//      if (!lookup.Advance(c))
//         return false;
//    }
//    return lookup.GetResultForCurrentSequence() != kDafsaNotFound;
//
// Example usage for 'find longest prefix in set with result code == 3' query:
//
//    FixedSetIncrementalLookup prefix_lookup(kDafsa, sizeof(kDafsa));
//    size_t longest_match_end = 0;
//    for (size_t i = 0; i < input.length(); ++i) {
//      if (!prefix_lookup.Advance(input[i]))
//         break;
//      if (prefix_lookup.GetResultForCurrentSequence() == 3)
//        longest_match_end = (i + 1);
//    }
//    return input.substr(0, longest_match_end);
//
class NET_EXPORT FixedSetIncrementalLookup {
 public:
  // Begin a lookup against the provided fixed set. |graph| and |length|
  // describe a byte buffer generated by the make_dafsa.py script, as described
  // in the class comment.
  //
  // FixedSetIncrementalLookup is initialized to a state corresponding to the
  // empty input sequence. Calling GetResultForCurrentSequence() in the initial
  // state would indicate whether the empty string appears in the fixed set.
  // Characters can be added to the sequence by calling Advance(), and the
  // lookup result can be checked after each addition by calling
  // GetResultForCurrentSequence().
  FixedSetIncrementalLookup(const unsigned char* graph, size_t length);

  // FixedSetIncrementalLookup is copyable so that callers can save/restore
  // their position in the search. This is for cases where branching or
  // backtracking might be required (e.g. to probe for the presence of a
  // designated wildcard character).
  FixedSetIncrementalLookup(const FixedSetIncrementalLookup&);
  FixedSetIncrementalLookup& operator=(const FixedSetIncrementalLookup&);

  ~FixedSetIncrementalLookup();

  // Advance the query by adding a character to the input sequence. |input| can
  // be any char value, but only ASCII characters will ever result in matches,
  // since the fixed set itself is limited to ASCII strings.
  //
  // Returns true if the resulting input sequence either appears in the fixed
  // set itself, or is a prefix of some longer string in the fixed set. Returns
  // false otherwise, implying that the graph is exhausted and
  // GetResultForCurrentSequence() will return kDafsaNotFound.
  //
  // Once Advance() has returned false, the caller can safely stop feeding more
  // characters, as subsequent calls to Advance() will return false and have no
  // effect.
  bool Advance(char input);

  // Returns the result code corresponding to the input sequence provided thus
  // far to Advance().
  //
  // If the sequence does not appear in the fixed set, the return value is
  // kDafsaNotFound. Otherwise, the value is a non-negative integer (currently
  // limited to 0-7) corresponding to the result code for that string, as listed
  // in the .gperf file from which the DAFSA was generated. For
  // GetDomainAndRegistry DAFSAs, these values should be interpreted as a
  // bitmask of kDafsaExceptionRule, kDafsaWildcardRule, and kDafsaPrivateRule.
  //
  // It is okay to call this function, and then extend the sequence further by
  // calling Advance().
  int GetResultForCurrentSequence() const;

 private:
  // Pointer to the current position in the graph indicating the current state
  // of the automaton, or nullptr if the graph is exhausted.
  const unsigned char* pos_;

  // Pointer just past the end of the graph. |pos_| should never get here. This
  // is used only in DCHECKs.
  const unsigned char* end_;

  // Contains the current decoder state. If true, |pos_| points to a label
  // character or a return code. If false, |pos_| points to a sequence of
  // offsets that indicate the child nodes of the current state.
  bool pos_is_label_character_;
};

}  // namespace net

#endif  // NET_BASE_LOOKUP_STRING_IN_FIXED_SET_H_
