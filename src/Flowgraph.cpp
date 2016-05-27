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

#include "Flowgraph.h"
#include "PcDebug.h"
#include <memory>

#if HAVE_DECL__MM_MALLOC
#   include <mm_malloc.h>
#else
#   define memalign(a, b)   malloc(b)
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

using namespace std;

typedef std::vector<shared_ptr<Node> >::iterator NodeIterator;
typedef std::vector<shared_ptr<Edge> >::iterator EdgeIterator;


Node::Node(shared_ptr<ModPlugin> plugin) :
    myPlugin(plugin),
    myProcessTime(0)
{
    PDEBUG("Node::Node(plugin(%s): %p) @ %p\n",
            plugin->name(), plugin.get(), this);

}


Node::~Node()
{
    PDEBUG("Node::~Node() @ %p\n", this);

    assert(myInputBuffers.size() == 0);
    assert(myOutputBuffers.size() == 0);
}


Edge::Edge(shared_ptr<Node>& srcNode, shared_ptr<Node>& dstNode) :
    mySrcNode(srcNode),
    myDstNode(dstNode)
{
    PDEBUG("Edge::Edge(srcNode(%s): %p, dstNode(%s): %p) @ %p\n",
            srcNode->plugin()->name(), srcNode.get(),
            dstNode->plugin()->name(), dstNode.get(),
            this);

    myBuffer = make_shared<Buffer>();
    srcNode->myOutputBuffers.push_back(myBuffer);
    dstNode->myInputBuffers.push_back(myBuffer);
}


Edge::~Edge()
{
    PDEBUG("Edge::~Edge() @ %p\n", this);

    std::vector<shared_ptr<Buffer> >::iterator buffer;
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
    }
}


int Node::process()
{
    PDEBUG("Edge::process()\n");
    PDEBUG(" Plugin name: %s (%p)\n", myPlugin->name(), myPlugin.get());

    // the plugin process() still wants vector<Buffer*>
    // arguments.
    std::vector<Buffer*> inBuffers;
    std::vector<shared_ptr<Buffer> >::iterator buffer;
    for (buffer = myInputBuffers.begin();
         buffer != myInputBuffers.end();
         ++buffer) {
        inBuffers.push_back(buffer->get());
    }

    std::vector<Buffer*> outBuffers;
    for (buffer = myOutputBuffers.begin();
         buffer != myOutputBuffers.end();
         ++buffer) {
        outBuffers.push_back(buffer->get());
    }

    return myPlugin->process(inBuffers, outBuffers);
}


Flowgraph::Flowgraph() :
    myProcessTime(0)
{
    PDEBUG("Flowgraph::Flowgraph() @ %p\n", this);

}


Flowgraph::~Flowgraph()
{
    PDEBUG("Flowgraph::~Flowgraph() @ %p\n", this);

    if (myProcessTime) {
        fprintf(stderr, "Process time:\n");

        for (const auto &node : nodes) {
            fprintf(stderr, "  %30s: %10lu us (%2.2f %%)\n",
                    node->plugin()->name(),
                    node->processTime(),
                    node->processTime() * 100.0 / myProcessTime);
        }

        fprintf(stderr, "  %30s: %10lu us (100.00 %%)\n", "total",
                myProcessTime);
    }
}

void Flowgraph::connect(shared_ptr<ModPlugin> input, shared_ptr<ModPlugin> output)
{
    PDEBUG("Flowgraph::connect(input(%s): %p, output(%s): %p)\n",
            input->name(), input.get(), output->name(), output.get());

    NodeIterator inputNode;
    NodeIterator outputNode;

    for (inputNode = nodes.begin(); inputNode != nodes.end(); ++inputNode) {
        if ((*inputNode)->plugin() == input) {
            break;
        }
    }
    if (inputNode == nodes.end()) {
        inputNode = nodes.insert(nodes.end(), make_shared<Node>(input));
    }

    for (outputNode = nodes.begin(); outputNode != nodes.end(); ++outputNode) {
        if ((*outputNode)->plugin() == output) {
            break;
        }
    }
    if (outputNode == nodes.end()) {
        outputNode = nodes.insert(nodes.end(), make_shared<Node>(output));
        for (inputNode = nodes.begin(); inputNode != nodes.end(); ++inputNode) {
            if ((*inputNode)->plugin() == input) {
                break;
            }
        }
    } else if (inputNode > outputNode) {
        shared_ptr<Node> node = *outputNode;
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

    edges.push_back(make_shared<Edge>(*inputNode, *outputNode));
}


bool Flowgraph::run()
{
    PDEBUG("Flowgraph::run()\n");

    timeval start, stop;
    time_t diff;

    gettimeofday(&start, NULL);
    for (const auto &node : nodes) {
        int ret = node->process();
        PDEBUG(" ret: %i\n", ret);
        gettimeofday(&stop, NULL);
        diff = (stop.tv_sec - start.tv_sec) * 1000000 +
            stop.tv_usec - start.tv_usec;
        myProcessTime += diff;
        node->addProcessTime(diff);
        start = stop;
        if (!ret) {
            return false;
        }
    }
    return true;
}

