/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2016
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
#include <algorithm>
#include <sstream>

#if HAVE_DECL__MM_MALLOC
#   include <mm_malloc.h>
#else
#   define memalign(a, b)   malloc(b)
#endif
#include <sys/types.h>
#include <stdexcept>
#include <assert.h>
#include <sys/time.h>

using namespace std;

using NodeIterator = std::vector<shared_ptr<Node> >::iterator;
using EdgeIterator = std::vector<shared_ptr<Edge> >::iterator;


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

void Node::addOutputBuffer(Buffer::sptr& buffer)
{
    myOutputBuffers.push_back(buffer);
#if DEBUG
    std::string fname = string(myPlugin->name()) +
        "-" + to_string(myDebugFiles.size()) +
        "-" + to_string((size_t)(void*)myPlugin.get()) +
        ".dat";
    FILE* fd = fopen(fname.c_str(), "wb");
    assert(fd != nullptr);
    myDebugFiles.push_back(fd);
#endif
}

void Node::removeOutputBuffer(Buffer::sptr& buffer)
{
    auto it = std::find(
            myOutputBuffers.begin(),
            myOutputBuffers.end(),
            buffer);
    if (it != myOutputBuffers.end()) {
#if DEBUG
        size_t pos = std::distance(myOutputBuffers.begin(),
                it);

        auto fd_it = std::next(myDebugFiles.begin(), pos);

        fclose(*fd_it);
        myDebugFiles.erase(fd_it);
#endif
        myOutputBuffers.erase(it);
    }
}

void Node::addInputBuffer(Buffer::sptr& buffer)
{
    myInputBuffers.push_back(buffer);
}

void Node::removeInputBuffer(Buffer::sptr& buffer)
{
    auto it = std::find(
            myInputBuffers.begin(),
            myInputBuffers.end(),
            buffer);
    if (it != myInputBuffers.end()) {
        myInputBuffers.erase(it);
    }
}

int Node::process()
{
    PDEBUG("Node::process()\n");
    PDEBUG(" Plugin name: %s (%p)\n", myPlugin->name(), myPlugin.get());

    // the plugin process() wants vector<Buffer*>
    // arguments.
    std::vector<Buffer*> inBuffers;
    for (auto& buffer : myInputBuffers) {
        assert(buffer.get() != nullptr);
        inBuffers.push_back(buffer.get());
    }

    std::vector<Buffer*> outBuffers;
    for (auto& buffer : myOutputBuffers) {
        assert(buffer.get() != nullptr);
        outBuffers.push_back(buffer.get());
    }

    int ret = myPlugin->process(inBuffers, outBuffers);
#if DEBUG
    assert(myDebugFiles.size() == myOutputBuffers.size());

    auto buf   = myOutputBuffers.begin();
    auto fd_it = myDebugFiles.begin();

    for (size_t i = 0; i < myDebugFiles.size(); i++) {
        if (*buf) {
            Buffer& b = *buf->get();
            FILE* fd = *fd_it;

            fwrite(b.getData(), b.getLength(), 1, fd);
        }

        ++buf;
        ++fd_it;
    }
#endif
    return ret;
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
    srcNode->addOutputBuffer(myBuffer);
    dstNode->addInputBuffer(myBuffer);
}


Edge::~Edge()
{
    PDEBUG("Edge::~Edge() @ %p\n", this);

    if (myBuffer) {
        mySrcNode->removeOutputBuffer(myBuffer);
        myDstNode->removeInputBuffer(myBuffer);
    }
}



Flowgraph::Flowgraph() :
    myProcessTime(0)
{
    PDEBUG("Flowgraph::Flowgraph() @ %p\n", this);
}


Flowgraph::~Flowgraph()
{
    PDEBUG("Flowgraph::~Flowgraph() @ %p\n", this);

    stringstream ss;

    if (myProcessTime) {
        ss << "Process time:\n";

        char node_time_sz[1024] = {};

        for (const auto &node : nodes) {
            snprintf(node_time_sz, 1023, "  %30s: %10lu us (%2.2f %%)\n",
                    node->plugin()->name(),
                    node->processTime(),
                    node->processTime() * 100.0 / myProcessTime);
            ss << node_time_sz;
        }

        snprintf(node_time_sz, 1023, "  %30s: %10lu us (100.00 %%)\n", "total",
                myProcessTime);
        ss << node_time_sz;

        fprintf(stderr, "%s", ss.str().c_str());
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

