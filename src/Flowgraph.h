/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2015
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

#ifndef FLOWGRAPH_H
#define FLOWGRAPH_H

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#include "porting.h"
#include "ModPlugin.h"


#include <sys/types.h>
#include <vector>
#include <boost/shared_ptr.hpp>


class Node
{
public:
    Node(boost::shared_ptr<ModPlugin> plugin);
    ~Node();
    Node(const Node&);
    Node& operator=(const Node&);

    boost::shared_ptr<ModPlugin> plugin() { return myPlugin; }

    std::vector<boost::shared_ptr<Buffer> > myInputBuffers;
    std::vector<boost::shared_ptr<Buffer> > myOutputBuffers;

    int process();
    time_t processTime() { return myProcessTime; }
    void addProcessTime(time_t processTime) {
        myProcessTime += processTime;
    }

protected:
    boost::shared_ptr<ModPlugin> myPlugin;
    time_t myProcessTime;
};


class Edge
{
public:
    Edge(boost::shared_ptr<Node>& src, boost::shared_ptr<Node>& dst);
    ~Edge();
    Edge(const Edge&);
    Edge& operator=(const Edge&);

protected:
    boost::shared_ptr<Node> mySrcNode;
    boost::shared_ptr<Node> myDstNode;
    boost::shared_ptr<Buffer> myBuffer;
};


class Flowgraph
{
public:
    Flowgraph();
    virtual ~Flowgraph();
    Flowgraph(const Flowgraph&);
    Flowgraph& operator=(const Flowgraph&);

    void connect(boost::shared_ptr<ModPlugin> input,
                 boost::shared_ptr<ModPlugin> output);
    bool run();

protected:
    std::vector<boost::shared_ptr<Node> > nodes;
    std::vector<boost::shared_ptr<Edge> > edges;
    time_t myProcessTime;
};


#endif // FLOWGRAPH_H

