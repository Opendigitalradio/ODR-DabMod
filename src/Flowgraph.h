/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2018
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://opendigitalradio.org
 */
/*
   This file is part of ODR-DabMod.

   ODR-DabMod is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   ODR-DabMod is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with ODR-DabMod.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#include "ModPlugin.h"

#include <memory>
#include <sys/types.h>
#include <vector>
#include <list>
#include <cstdio>

using Metadata_vec_sptr = std::shared_ptr<std::vector<flowgraph_metadata> >;

class Node
{
public:
    Node(std::shared_ptr<ModPlugin> plugin);
    ~Node();
    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;

    std::shared_ptr<ModPlugin> plugin() { return myPlugin; }

    int process();
    time_t processTime() const;
    void addProcessTime(time_t time);

    void addOutputBuffer(Buffer::sptr& buffer, Metadata_vec_sptr& md);
    void removeOutputBuffer(Buffer::sptr& buffer, Metadata_vec_sptr& md);

    void addInputBuffer(Buffer::sptr& buffer, Metadata_vec_sptr& md);
    void removeInputBuffer(Buffer::sptr& buffer, Metadata_vec_sptr& md);

protected:
    std::list<Buffer::sptr> myInputBuffers;
    std::list<Buffer::sptr> myOutputBuffers;
    std::list<Metadata_vec_sptr> myInputMetadata;
    std::list<Metadata_vec_sptr> myOutputMetadata;

#if TRACE
    std::list<FILE*> myDebugFiles;
#endif

    std::shared_ptr<ModPlugin> myPlugin;
    time_t myProcessTime = 0;
};


class Edge
{
public:
    Edge(std::shared_ptr<Node>& src, std::shared_ptr<Node>& dst);
    ~Edge();
    Edge(const Edge&) = delete;
    Edge& operator=(const Edge&) = delete;

protected:
    std::shared_ptr<Node> mySrcNode;
    std::shared_ptr<Node> myDstNode;
    std::shared_ptr<Buffer> myBuffer;
    std::shared_ptr<std::vector<flowgraph_metadata> > myMetadata;
};


class Flowgraph
{
public:
    Flowgraph(bool showProcessTime);
    virtual ~Flowgraph();
    Flowgraph(const Flowgraph&) = delete;
    Flowgraph& operator=(const Flowgraph&) = delete;

    void connect(std::shared_ptr<ModPlugin> input,
                 std::shared_ptr<ModPlugin> output);
    bool run();

protected:
    std::vector<std::shared_ptr<Node> > nodes;
    std::vector<std::shared_ptr<Edge> > edges;
    time_t myProcessTime = 0;
    bool myShowProcessTime;
};


