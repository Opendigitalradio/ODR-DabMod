/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)
 */
/*
   This file is part of CRC-DADMOD.

   CRC-DADMOD is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   CRC-DADMOD is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with CRC-DADMOD.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef FLOWGRAPH_H
#define FLOWGRAPH_H

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#include "porting.h"
#include "ModPlugin.h"


#include <sys/types.h>
#include <vector>


class Node
{
public:
    Node(ModPlugin* plugin);
    ~Node();
    Node(const Node&);
    Node& operator=(const Node&);

    ModPlugin* plugin() { return myPlugin; }

    std::vector<Buffer*> myInputBuffers;
    std::vector<Buffer*> myOutputBuffers;

    int process();
    time_t processTime() { return myProcessTime; }
    void addProcessTime(time_t processTime) {
        myProcessTime += processTime;
    }

protected:
    ModPlugin* myPlugin;
    time_t myProcessTime;
};


class Edge
{
public:
    Edge(Node* src, Node* dst);
    ~Edge();
    Edge(const Edge&);
    Edge& operator=(const Edge&);

protected:
    Node* mySrcNode;
    Node* myDstNode;
    Buffer* myBuffer;
};


class Flowgraph
{
public:
    Flowgraph();
    virtual ~Flowgraph();
    Flowgraph(const Flowgraph&);
    Flowgraph& operator=(const Flowgraph&);

    void connect(ModPlugin* input, ModPlugin* output);
    bool run();

protected:
    std::vector<Node*> nodes;
    std::vector<Edge*> edges;
    time_t myProcessTime;
};


#endif // FLOWGRAPH_H
