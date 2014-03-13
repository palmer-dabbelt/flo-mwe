/*
 * Copyright (C) 2014 Palmer Dabbelt
 *   <palmer.dabbelt@eecs.berkeley.edu>
 *
 * This file is part of flo-mwe.
 *
 * flo-mwe is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * flo-mwe is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with flo-mwe.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef WIDE_NODE_HXX
#define WIDE_NODE_HXX

class wide_node;

#include "narrow_node.h++"
#include <libflo/node.h++>
#include <memory>
#include <vector>

/* Holds a node that may be too wide to fit inside a single operation.
 * This is a different type from a narrow node to ensure that you
 * never accidentally mix the two. */
class wide_node: public libflo::node {
    friend class libflo::node;

public:
    class nnode_viter {
    private:
        const typename std::vector<std::shared_ptr<narrow_node>> _nodes;
        typename std::vector<std::shared_ptr<narrow_node>>::const_iterator _it;
    public:
        nnode_viter(const std::vector<std::shared_ptr<narrow_node>>& nodes)
            : _nodes(nodes),
              _it(_nodes.begin())
            {
            }
        std::shared_ptr<narrow_node> operator*(void) const { return *_it; }
        bool done(void) const { return _it == _nodes.end(); }
        void operator++(void) { ++_it; }
    };

private:
    static size_t _word_length;
    static bool _word_length_set;

private:
    /* Stores the set of narrow words that coorespond to this wide
     * word. */
    std::vector<std::shared_ptr<narrow_node>> _nns;
    bool _nns_valid;

    /* The list of nodes to chain together when doing a debugging
     * cat. */
    std::vector<std::shared_ptr<narrow_node>> _cdn;
    bool _cdn_valid;

private:
    wide_node(const std::string name,
              const libflo::unknown<size_t>& width,
              const libflo::unknown<size_t>& depth,
              bool is_mem,
              bool is_const,
              libflo::unknown<size_t> cycle);

public:
    /* Returns an iterator that walks through the list of narrow nodes
     * that would need to be created in order to implement this node
     * on a narrow machine. */
    nnode_viter nnodes(void);

    /* Returns a single one of the narrow nodes. */
    std::shared_ptr<narrow_node> nnode(size_t i);
    size_t nnode_count(void) const { return _nns.size(); }

    /* Here's the list of CATD nodes that serve to produce the actual
     * output node. */
    std::shared_ptr<narrow_node> catdnode(size_t i);

public:
    /* Sets a global width parameter that refers to all nodes and
     * determines the word size of the machine that's being generated
     * for.  Note that this can only be set once during the life of
     * the program!  There's also an operation to obtain this word
     * length. */
    static void set_word_length(size_t word_length);
    static size_t get_word_length(void);
};

#endif
