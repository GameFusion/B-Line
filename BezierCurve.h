#ifndef BEZIERCURVE_H
#define BEZIERCURVE_H

#include <QJsonObject>

#include <vector>
#include <QPointF>
#include "StrokeProperties.h"
#include "Vector3D.h"

namespace GameFusion {

struct BezierControl {
    Vector3D point;      // Anchor point
    Vector3D leftControl; // Left control point (relative to point)
    Vector3D rightControl; // Right control point (relative to point)

    BezierControl(const Vector3D& p, const Vector3D& left, const Vector3D& right)
        : point(p), leftControl(left), rightControl(right) {}
};

struct IntersectionInfo {
    Vector3D point; // Intersection point
    float t;        // Global t-value (0 to 1) across the curve
};

class BezierCurve {
public:
    BezierCurve() = default;
    explicit BezierCurve(const std::vector<Vector3D>& vertices);

    // Add a Bezier handle to the curve
    BezierCurve& operator+=(const BezierControl& handle);

    // Subscript operator for accessing handles
    BezierControl& operator[](size_t index) {
        return handles_[index];
    }
    const BezierControl& operator[](size_t index) const {
        return handles_[index];
    }

    // Clear all vertices and handles
    void clear();

    // Assess the curve, generating vertices for rendering (stepCount: points per segment, closed: loop the curve)
    void assess(int stepCount, bool closed);

    // Get the vertex array for rendering
    std::vector<Vector3D>& vertexArray() { return vertices_; }
    const std::vector<Vector3D>& vertexArray() const { return vertices_; }

    // Static method to interpolate a value along the curve (for animation)
    static float GetValue(float startTime, float endTime, float startValue, float endValue,
                          float control1, float control2, float currentTime);

    // JSON serialization (for integration with Layer)
    void toJson(QJsonObject& json) const;
    void fromJson(const QJsonObject& json);

    int size() const {return handles_.size();};

    // Get and set stroke properties
    void setStrokeProperties(const StrokeProperties& props) {
        strokeProperties_ = props;
    }
    const StrokeProperties& getStrokeProperties() const {
        return strokeProperties_;
    }

    // Iterator support
    auto begin() { return handles_.begin(); }
    auto end() { return handles_.end(); }
    auto begin() const { return handles_.begin(); }
    auto end() const { return handles_.end(); }

private:
    std::vector<BezierControl> handles_; // Control points defining the curve
    std::vector<Vector3D> vertices_;    // Generated vertices for rendering
    StrokeProperties strokeProperties_; // Per-stroke attributes

};

bool sectionCurveAtIntersection(const BezierCurve& curve, const BezierCurve& other, std::pair<BezierCurve, BezierCurve>& sectioned, IntersectionInfo& info);


} // namespace GameFusion

#endif // BEZIERCURVE_H
