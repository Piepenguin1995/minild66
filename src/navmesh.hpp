#ifndef NAVMESH_HPP
#define NAVMESH_HPP

#include <SFML/System.hpp>
#include <vector>
#include <cmath>
#include <JsonBox.h>

#include "graph.hpp"
#include "vecmath.hpp"

class ConvexPolygon
{
public:
    // Points defined in an anticlockwise direction
    // First point must be ac of the positive horizontal
    std::vector<sf::Vector2f> points;

    // Helper function to add points
    void add(float x, float y)
    {
        points.push_back(sf::Vector2f(x, y));
    }

    // Loop through the points in anticlockwise order and draw
    // a vector between consecutive points. If the points is
    // left of every vector, it's in the polygon
    bool contains(const sf::Vector2f& point) const
    {
        for(int i = 0; i < points.size(); ++i)
        {
            auto p1 = points[i];
            auto p2 = points[(i+1)%points.size()];
            // To check the side of the line AB, take AB x AX
            // and look at the sign. If +ve, by the right hand rule
            // the point X is to the left of AB
            auto v = p2 - p1;
            auto u = point - p1;
            if(v.x*u.y - v.y*u.x < 0) return false;
        }

        return true;
    }

    sf::Vector2f centroid() const
    {
        sf::Vector2f cent(0.0f, 0.0f);
        for(int i = 0; i < points.size(); ++i)
        {
            // Assume that polygon is sensibly sized and has
            // few vertices, so that totalling the positions
            // does not introduce precision errors
            cent += points[i];
        }
        return cent / (float)points.size();
    }

    ConvexPolygon intersect(const ConvexPolygon& b) const
    {
        // Use a variant of the Sutherland-Hodgman algorithm
        // to calculate the intersection between two
        // convex polygons (which is again a convex polygon)
        ConvexPolygon clipped;
        for(int i = 0; i < b.points.size(); ++i)
        {
            auto p0 = b.points[i];
            // If A contains p0, p0 lies in the intersection
            if(this->contains(p0)) clipped.points.push_back(p0);
            // The line joining p_i and p_(i+1) will intersect
            // with A iff p_(i+1) lies in A and p_i does not.
            // If there is no intersection then the point can be ignored and
            // does not affect the clipped polygon.
            auto p1 = b.points[(i+1)%b.points.size()];
            if(this->contains(p0) && this->contains(p1)) continue;
            else if(!this->contains(p0) && !this->contains(p1)) continue;
            // Find the edge of node which intersects with the
            // edge of poly
            for(int j = 0; j < this->points.size(); ++j)
            {
                auto q0 = this->points[j];
                auto q1 = this->points[(j+1)%this->points.size()];
                sf::Vector2f intersection;
                if(vecmath::intersect(p0, p1, q0, q1, &intersection))
                {
                    // Add the intersection as a point
                    clipped.points.push_back(intersection);
                    break;
                }
            }
        }
        return clipped;
    }
};

// A Navmesh is a graph of convex polygons
class Navmesh
{
public:
    Graph<ConvexPolygon> polys;

    Navmesh() {}
    explicit Navmesh(const JsonBox::Value& v)
    {
        load(v);
    }

    void load(const JsonBox::Value& v)
    {
        JsonBox::Object o = v.getObject();
        if(o.find("nodes") != o.end())
        {
            JsonBox::Array nodes = o["nodes"].getArray();
            for(auto node : nodes)
            {
                ConvexPolygon poly;
                JsonBox::Array nodePoints = node.getArray();
                for(auto point : nodePoints)
                {
                    JsonBox::Array components = point.getArray();
                    // TODO: Array bounds checking
                    poly.points.push_back(sf::Vector2f(
                                components[0].tryGetFloat(0.0f),
                                components[1].tryGetFloat(0.0f)));
                }
                polys.add_node(poly);
            }
        }
        if(o.find("edges") != o.end())
        {
            JsonBox::Array edges = o["edges"].getArray();
            for(auto edge : edges)
            {
                // TODO: Array bounds checking
                JsonBox::Array components = edge.getArray();
                polys.add_edge(
                        components[0].tryGetInteger(0),
                        components[1].tryGetInteger(0),
                        false);
            }
        }
    }

    // Subtract a convex polygon from the navmesh
    void subtract(const ConvexPolygon& poly)
    {
        const sf::Vector2f centroid = poly.centroid();
        std::vector<int> nodesIntersectingPoly;
        // Poly may lie in more than one node, so first find
        // every node that poly lies in
        for(auto p : poly.points)
        {
            for(int i = 0; i < polys.nodes.size(); ++i)
            {
                if(polys.nodes[i].data.contains(p))
                {
                    nodesIntersectingPoly.push_back(i);
                    break;
                }
            }
        }
        // Now for every node which contained at least part of poly,
        // replace poly with the intersection of poly and the node.
        // Note that the union of two convex polygons is again a
        // convex polygon, so this is valid.
        for(auto i : nodesIntersectingPoly)
        {
            const ConvexPolygon& node = polys.nodes[i].data;
            ConvexPolygon clippedPolygon;
            // Save some time by not performing polygon clipping if
            // poly lies entirely within node
            bool allIn = true;
            for(int j = 0; j < poly.points.size(); ++j)
            {
                if(!node.contains(poly.points[j]))
                {
                    allIn = false;
                    break;
                }
            }
            if(!allIn)
            {
                // Polygon intersects so need its intersection
            }
        }
    }
};

#endif /* NAVMESH_HPP */
