#ifndef BEZIERCURVE_H
#define BEZIERCURVE_H

#include <QJsonObject>

#include <vector>
#include <QPointF>
#include "StrokeProperties.h"
#include "Vector3D.h"

namespace GameFusion {

struct StrokePoint {
    QPointF pos;
    float pressure = 1.0f;  // 0.0-1.0
};

struct BezierControl {

    enum class HandleMode {
        AutoSmooth,     // Automatically smoothed tangent
        Linear,         // Straight-line interpolation
        Manual          // Explicitly user-controlled
    };

    Vector3D point;      // Anchor point
    Vector3D leftControl; // Left control point (relative to point)
    Vector3D rightControl; // Right control point (relative to point)

    Vector3D sideNormal; // side normal to the curve
    Vector3D tangentNormal; // tangent normal to the curve

    int indexPoint = -1;

    HandleMode leftMode = HandleMode::Manual;
    HandleMode rightMode = HandleMode::Manual;

    BezierControl(const Vector3D& p, const Vector3D& left, const Vector3D& right)
        : point(p), leftControl(left), rightControl(right) {}

    BezierControl(const Vector3D& p, const Vector3D& left, const Vector3D& right, const int indexPoint)
        : point(p), leftControl(left), rightControl(right), indexPoint(indexPoint) {}

    BezierControl(){}
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

    std::vector<float> strokePressure(){return strokePressure_;}
    std::vector<float> strokePressure() const {return strokePressure_;}

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

    void setStrokePressure(const std::vector<float> pressureData){
        strokePressure_ = pressureData;
    }

    // Iterator support
    auto begin() { return handles_.begin(); }
    auto end() { return handles_.end(); }
    auto begin() const { return handles_.begin(); }
    auto end() const { return handles_.end(); }

    Vector3D evaluate(double t) const;

    Vector3D getAnchor(int index) const;

    // Smoothing: Creates smooth in/out tangents with uniform time spacing *per segment*
    // strength: 0.0 = linear, 1.0 = full smooth (default 0.5)
    void smoothAuto(bool loop=false);
    void smooth(float strength = 0.5f, bool loop = false);
    void smooth2(float smoothIn, float smoothOut);

    // Linearize: Creates straight-line tangents with uniform linear spacing *per segment*
    void linearize(bool loop = false);

    // Helper: Compute perpendicular vector (normal) in 2D for smooth tangents
    static Vector3D perpendicular(const Vector3D& vec) { return Vector3D(-vec.y(), vec.x()); }

private:
    std::vector<BezierControl>  handles_; // Control points defining the curve
    std::vector<Vector3D>       vertices_;    // Generated vertices for rendering
    StrokeProperties            strokeProperties_; // Per-stroke attributes
    std::vector<float>          strokePressure_; // tablet pressure

    bool m_isClosed = false;
};

bool sectionCurveAtIntersection(const BezierCurve& curve, const BezierCurve& other, std::pair<BezierCurve, BezierCurve>& sectioned, IntersectionInfo& info);


// Simple 2x2 matrix inversion for least-squares
struct Matrix2x2 {
    double a, b, c, d;
};


bool invert2x2(const Matrix2x2& m, Matrix2x2& inv);

// Solve Ax = b for 2x2 A, 2D x/b
Vector3D solve2x2(const Matrix2x2& A, double bx, double by);

// Find closest t on curve to point Q (Newton-Raphson)
double closestParam(const GameFusion::BezierCurve& curve, const QPointF& Q, double initial_t = 0.5);

// Compute chord-length parameters
std::vector<double> chordParams(const std::vector<StrokePoint>& points);

BezierCurve simplifyBezierCurve(const BezierCurve& curve, double threshold);

} // namespace GameFusion



#endif // BEZIERCURVE_H
