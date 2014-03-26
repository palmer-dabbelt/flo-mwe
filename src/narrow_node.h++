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

#ifndef NARROW_NODE_HXX
#define NARROW_NODE_HXX

class narrow_node;

#include "shallow_node.h++"
#include "wide_node.h++"
#include <libflo/node.h++>

/* Holds a narrow node, which is a node that will always fit within a
 * single word on the target machine. */
class narrow_node: public libflo::node {
    friend class libflo::node;

public:
    class snode_viter {
    private:
        const typename std::vector<std::shared_ptr<shallow_node>> _nodes;
        typename std::vector<std::shared_ptr<shallow_node>>::const_iterator _it;
    public:
        snode_viter(const std::vector<std::shared_ptr<shallow_node>>& nodes)
            : _nodes(nodes),
              _it(_nodes.begin())
            {
            }
        std::shared_ptr<shallow_node> operator*(void) const { return *_it; }
        bool done(void) const { return _it == _nodes.end(); }
        void operator++(void) { ++_it; }
    };

private:
    /* Stores the set of shallow words that coorespond to this narrow
     * word. */
    std::vector<std::shared_ptr<shallow_node>> _sns;
    bool _sns_valid;

public:
    narrow_node(const std::string name,
                const libflo::unknown<size_t>& width,
                const libflo::unknown<size_t>& depth,
                bool is_mem,
                bool is_const,
                libflo::unknown<size_t> cycle);

    narrow_node(const std::string name,
                const libflo::unknown<size_t>& width,
                const libflo::unknown<size_t>& depth,
                bool is_mem,
                bool is_const,
                libflo::unknown<size_t> cycle,
                bool is_catd);

public:
    /* Returns an iterator that walks through the list of shallow nodes
     * that would need to be created in order to implement this node
     * on a shallow machine. */
    snode_viter snodes(void);
    std::shared_ptr<shallow_node> snode(size_t i);

public:
    /* Clones a wide node into a narrow node. */
    static std::shared_ptr<narrow_node>
    clone_from(std::shared_ptr<wide_node> w);

    /* Allows us to force the creation of a narrow node that's
     * actually wide.  This is actually a pretty unsafe operation and
     * should probably be deprecated, but it's necessary to support IN
     * nodes... */
    static std::shared_ptr<narrow_node>
    clone_from(std::shared_ptr<wide_node> w, bool force);

    /* Creates a automatically named temporary node based on the
     * template of another node.  The idea is that this new node has
     * all the same properties but is otherwise not associated with
     * the original node. */
    static std::shared_ptr<narrow_node>
    create_temp(const std::shared_ptr<narrow_node> tplt);

    static std::shared_ptr<narrow_node>
    create_temp(const size_t width);

    /* Creates a constant, otherwise copying from the template like
     * above. */
    static std::shared_ptr<narrow_node>
    create_const(const std::shared_ptr<narrow_node> tplt, size_t value);
};

#endif
