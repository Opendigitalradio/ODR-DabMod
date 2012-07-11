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

#include "Flowgraph.h"
#include "PcDebug.h"


#ifdef __ppc__
#   define memalign(a, b)   malloc(b)
#else // !__ppc__
#   include <mm_malloc.h>
#endif
#include <sys/types.h>
#include <stdexcept>
#include <assert.h>
#if defined(_WIN32) and !defined(__MINGW32__)
//#include <sys/timeb.h>
//#include <sys/types.h>
#else
#include <sys/time.h>
#endif


typedef std::vector<Node*>::iterator NodeIterator;
typedef std::vector<Edge*>::iterator EdgeIterator;


Node::Node(ModPlugin* plugin) :
    myPlugin(plugin),
    myProcessTime(0)
{
    PDEBUG("Node::Node(plugin(%s): %p) @ %p\n", plugin->name(), plugin, this);

}


Node::~Node()
{
    PDEBUG("Node::~Node() @ %p\n", this);

    if (myPlugin != NULL) {
        delete myPlugin;
    }
    assert(myInputBuffers.size() == 0);
    assert(myOutputBuffers.size() == 0);
}


Edge::Edge(Node* srcNode, Node* dstNode) :
    mySrcNode(srcNode),
    myDstNode(dstNode)
{
    PDEBUG("Edge::Edge(srcNode(%s): %p, dstNode(%s): %p) @ %p\n",
            srcNode->plugin()->name(), srcNode,
            dstNode->plugin()->name(), dstNode,
            this);

    myBuffer = new Buffer();
    srcNode->myOutputBuffers.push_back(myBuffer);
    dstNode->myInputBuffers.push_back(myBuffer);
}


Edge::~Edge()
{
    PDEBUG("Edge::~Edge() @ %p\n", this);

    std::vector<Buffer*>::iterator buffer;
    if (myBuffer != NULL) {
        for (buffer = mySrcNode->myOutputBuffers.begin();
                buffer != mySrcNode->myOutputBuffers.end();
                ++buffer) {
            if (*buffer == myBuffer) {
                mySrcNode->myOutputBuffers.erase(buffer);
                break;
            }
        }

        for (buffer = myDstNode->myInputBuffers.begin();
                buffer != myDstNode->myInputBuffers.end();
                ++buffer) {
            if (*buffer == myBuffer) {
                myDstNode->myInputBuffers.erase(buffer);
                break;
            }
        }
        delete myBuffer;
    }
}


int Node::process()
{
    PDEBUG("Edge::process()\n");
    PDEBUG(" Plugin name: %s (%p)\n", myPlugin->name(), myPlugin);

    return myPlugin->process(myInputBuffers, myOutputBuffers);
}


Flowgraph::Flowgraph() :
    myProcessTime(0)
{
    PDEBUG("Flowgraph::Flowgraph() @ %p\n", this);

}


Flowgraph::~Flowgraph()
{
    PDEBUG("Flowgraph::~Flowgraph() @ %p\n", this);

    std::vector<Edge*>::const_iterator edge;
    for (edge = edges.begin(); edge != edges.end(); ++edge) {
        delete *edge;
    }

    if (myProcessTime) {
        fprintf(stderr, "Process time:\n");
    }
    std::vector<Node*>::const_iterator node;
    for (node = nodes.begin(); node != nodes.end(); ++node) {
        if (myProcessTime) {
            fprintf(stderr, "  %30s: %10u us (%2.2f %%)\n",
                    (*node)->plugin()->name(),
                    (unsigned)(*node)->processTime(),
                    (*node)->processTime() * 100.0 / myProcessTime);
        }
        delete *node;
    }
    if (myProcessTime) {
        fprintf(stderr, "  %30s: %10u us (100.00 %%)\n", "total",
                (unsigned)myProcessTime);
    }
}


void Flowgraph::connect(ModPlugin* input, ModPlugin* output)
{
    PDEBUG("Flowgraph::connect(input(%s): %p, output(%s): %p)\n",
            input->name(), input, output->name(), output);

    NodeIterator inputNode;
    NodeIterator outputNode;

    for (inputNode = nodes.begin(); inputNode != nodes.end(); ++inputNode) {
        if ((*inputNode)->plugin() == input) {
            break;
        }
    }
    if (inputNode == nodes.end()) {
        inputNode = nodes.insert(nodes.end(), new Node(input));
    }

    for (outputNode = nodes.begin(); outputNode != nodes.end(); ++outputNode) {
        if ((*outputNode)->plugin() == output) {
            break;
        }
    }
    if (outputNode == nodes.end()) {
        outputNode = nodes.insert(nodes.end(), new Node(output));
        for (inputNode = nodes.begin(); inputNode != nodes.end(); ++inputNode) {
            if ((*inputNode)->plugin() == input) {
                break;
            }
        }
    } else if (inputNode > outputNode) {
        Node* node = *outputNode;
        nodes.erase(outputNode);
        outputNode = nodes.insert(nodes.end(), node);
        for (inputNode = nodes.begin(); inputNode != nodes.end(); ++inputNode) {
            if ((*inputNode)->plugin() == input) {
                break;
            }
        }
    }

    assert((*inputNode)->plugin() == input);
    assert((*outputNode)->plugin() == output);

    edges.push_back(new Edge(*inputNode, *outputNode));
}


bool Flowgraph::run()
{
    PDEBUG("Flowgraph::run()\n");

    std::vector<Node*>::const_iterator node;
    timeval start, stop;
    time_t diff;

    gettimeofday(&start, NULL);
    for (node = nodes.begin(); node != nodes.end(); ++node) {
        int ret = (*node)->process();
        PDEBUG(" ret: %i\n", ret);
        gettimeofday(&stop, NULL);
        diff = (stop.tv_sec - start.tv_sec) * 1000000 +
            stop.tv_usec - start.tv_usec;
        myProcessTime += diff;
        (*node)->addProcessTime(diff);
        start = stop;
        if (!ret) {
            return false;
        }
    }
    return true;
}
