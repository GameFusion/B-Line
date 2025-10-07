#include "BezierCurve.h"
#include <QJsonObject>
#include <QJsonArray>
#include <cmath>

namespace GameFusion {

BezierCurve::BezierCurve(const std::vector<Vector3D>& vertices) : vertices_(vertices) {}

BezierCurve& BezierCurve::operator+=(const BezierControl& handle) {
    handles_.push_back(handle);
    return *this;
}


void BezierCurve::clear() {
    handles_.clear();
    vertices_.clear();
    strokeProperties_ = StrokeProperties();
}

void BezierCurve::assess(int stepCount, bool closed) {
    vertices_.clear();
    if (handles_.empty()) return;

    // Generate vertices for each segment
    for (size_t i = 0; i < handles_.size() - (closed ? 0 : 1); ++i) {
        const BezierControl& h1 = handles_[i];
        const BezierControl& h2 = handles_[(i + 1) % handles_.size()];

        Vector3D p1 = h1.point;
        Vector3D c1 = h1.point + h1.rightControl;
        Vector3D c2 = h2.point + h2.leftControl;
        Vector3D p2 = h2.point;

        for (int t = 0; t <= stepCount; ++t) {
            float u = static_cast<float>(t) / stepCount;
            float u2 = u * u;
            float u3 = u2 * u;
            float mu = 1.0f - u;
            float mu2 = mu * mu;
            float mu3 = mu2 * mu;

            // Cubic Bezier interpolation
            float x = mu3 * p1.x() + 3 * mu2 * u * c1.x() + 3 * mu * u2 * c2.x() + u3 * p2.x();
            float y = mu3 * p1.y() + 3 * mu2 * u * c1.y() + 3 * mu * u2 * c2.y() + u3 * p2.y();
            vertices_.emplace_back(x, y, 0.0f);
        }
    }

    if (closed && !handles_.empty()) {
        vertices_.push_back(handles_[0].point); // Close the loop
    }
}

float BezierCurve::GetValue(float startTime, float endTime, float startValue, float endValue,
                            float control1, float control2, float currentTime) {
    if (currentTime <= startTime) return startValue;
    if (currentTime >= endTime) return endValue;

    float t = (currentTime - startTime) / (endTime - startTime);
    float u = 1.0f - t;
    float t2 = t * t;
    float u2 = u * u;

    // Quadratic Bezier for animation (simplified, as in original)
    return u2 * startValue + 2 * u * t * control1 + t2 * endValue;
}


void BezierCurve::toJson(QJsonObject& json) const {
    QJsonArray handlesArray;
    for (const auto& handle : handles_) {
        QJsonObject handleObj;
        handleObj["point"] = QJsonObject{
            {"x", handle.point.x()},
            {"y", handle.point.y()},
            {"z", handle.point.z()}
        };
        handleObj["leftControl"] = QJsonObject{
            {"x", handle.leftControl.x()},
            {"y", handle.leftControl.y()},
            {"z", handle.leftControl.z()}
        };
        handleObj["rightControl"] = QJsonObject{
            {"x", handle.rightControl.x()},
            {"y", handle.rightControl.y()},
            {"z", handle.rightControl.z()}
        };
        handlesArray.append(handleObj);
    }
    json["handles"] = handlesArray;

    // Save stroke properties
    QJsonObject strokeProps;
    strokeProps["smoothness"] = strokeProperties_.smoothness;
    strokeProps["maxWidth"] = strokeProperties_.maxWidth;
    strokeProps["minWidth"] = strokeProperties_.minWidth;
    strokeProps["variableWidthMode"] = static_cast<int>(strokeProperties_.variableWidthMode);
    strokeProps["stepCount"] = strokeProperties_.stepCount;
    strokeProps["foregroundColor"] = QJsonObject{
        {"r", strokeProperties_.foregroundColor.red()},
        {"g", strokeProperties_.foregroundColor.green()},
        {"b", strokeProperties_.foregroundColor.blue()},
        {"a", strokeProperties_.foregroundColor.alpha()}
    };
    strokeProps["backgroundColor"] = QJsonObject{
        {"r", strokeProperties_.backgroundColor.red()},
        {"g", strokeProperties_.backgroundColor.green()},
        {"b", strokeProperties_.backgroundColor.blue()},
        {"a", strokeProperties_.backgroundColor.alpha()}
    };
    strokeProps["colorMode"] = static_cast<int>(strokeProperties_.colorMode);
    json["strokeProperties"] = strokeProps;
}

void BezierCurve::fromJson(const QJsonObject& json) {
    clear();

    // Load handles
    QJsonArray handlesArray = json["handles"].toArray();
    for (const auto& handleVal : handlesArray) {
        QJsonObject handleObj = handleVal.toObject();
        QJsonObject pointObj = handleObj["point"].toObject();
        QJsonObject leftControlObj = handleObj["leftControl"].toObject();
        QJsonObject rightControlObj = handleObj["rightControl"].toObject();

        Vector3D point(pointObj["x"].toDouble(), pointObj["y"].toDouble(), pointObj["z"].toDouble());
        Vector3D leftControl(leftControlObj["x"].toDouble(), leftControlObj["y"].toDouble(), leftControlObj["z"].toDouble());
        Vector3D rightControl(rightControlObj["x"].toDouble(), rightControlObj["y"].toDouble(), rightControlObj["z"].toDouble());

        handles_.emplace_back(point, leftControl, rightControl);
    }

    // Load stroke properties
    QJsonObject strokeProps = json["strokeProperties"].toObject();
    strokeProperties_.smoothness = strokeProps["smoothness"].toDouble(0.5);
    strokeProperties_.maxWidth = strokeProps["maxWidth"].toDouble(2.0);
    strokeProperties_.minWidth = strokeProps["minWidth"].toDouble(0.5);
    strokeProperties_.variableWidthMode = static_cast<StrokeProperties::VariableWidthMode>(
        strokeProps["variableWidthMode"].toInt(0));
    strokeProperties_.stepCount = strokeProps["stepCount"].toInt(20);
    QJsonObject fgColorObj = strokeProps["foregroundColor"].toObject();
    strokeProperties_.foregroundColor = QColor(
        fgColorObj["r"].toInt(0),
        fgColorObj["g"].toInt(0),
        fgColorObj["b"].toInt(0),
        fgColorObj["a"].toInt(255)
        );
    QJsonObject bgColorObj = strokeProps["backgroundColor"].toObject();
    strokeProperties_.backgroundColor = QColor(
        bgColorObj["r"].toInt(255),
        bgColorObj["g"].toInt(255),
        bgColorObj["b"].toInt(255),
        bgColorObj["a"].toInt(255)
        );
    strokeProperties_.colorMode = static_cast<StrokeProperties::ColorMode>(
        strokeProps["colorMode"].toInt(0));

    // Re-assess the curve with loaded step count
    assess(strokeProperties_.stepCount, false);
}

Vector3D GameFusion::BezierCurve::evaluate(double t) const {
    if (handles_.empty()) return {0, 0, 0};
    if (handles_.size() < 2) return handles_[0].point; // Single point case

    // Clamp t to [0, 1]
    t = std::clamp(t, 0.0, 1.0);

    // For a single segment (2 handles), use t directly
    if (handles_.size() == 2) {
        const Vector3D& P0 = handles_[0].point;
        const Vector3D C1 = handles_[0].point + handles_[0].rightControl;
        const Vector3D C2 = handles_[1].point + handles_[1].leftControl;
        const Vector3D& P3 = handles_[1].point;
        double u = 1.0 - t;
        return u * u * u * P0 + 3 * u * u * t * C1 + 3 * u * t * t * C2 + t * t * t * P3;
    }

    // For multiple segments, map t to the correct segment
    size_t numSegments = handles_.size() - 1;
    double segmentLength = 1.0 / numSegments;
    size_t segmentIndex = static_cast<size_t>(t * numSegments);
    if (segmentIndex >= numSegments) segmentIndex = numSegments - 1; // Handle t=1 edge case

    // Compute local t for the selected segment
    double localT = (t - segmentIndex * segmentLength) / segmentLength;

    // Get control points for the segment
    const Vector3D& P0 = handles_[segmentIndex].point;
    const Vector3D C1 = handles_[segmentIndex].point + handles_[segmentIndex].rightControl;
    const Vector3D C2 = handles_[segmentIndex + 1].point + handles_[segmentIndex + 1].leftControl;
    const Vector3D& P3 = handles_[segmentIndex + 1].point;

    // Compute point on the cubic Bézier segment
    double u = 1.0 - localT;
    return u * u * u * P0 + 3 * u * u * localT * C1 + 3 * u * localT * localT * C2 + localT * localT * localT * P3;
}

// Helper struct for a single cubic Bézier segment
struct CubicBezier {
    Vector3D p0, p1, p2, p3;

    Vector3D evaluate(float t) const {
        float mu = 1.0f - t;
        float mu2 = mu * mu;
        float mu3 = mu2 * mu;
        float t2 = t * t;
        float t3 = t2 * t;
        return mu3 * p0 + 3.0f * mu2 * t * p1 + 3.0f * mu * t2 * p2 + t3 * p3;
    }

    Vector3D derivative(float t) const {
        float mu = 1.0f - t;
        return 3.0f * mu * mu * (p1 - p0) + 6.0f * mu * t * (p2 - p1) + 3.0f * t * t * (p3 - p2);
    }

    std::pair<CubicBezier, CubicBezier> split(float t) const {
        Vector3D a1 = (1.0f - t) * p0 + t * p1;
        Vector3D a2 = (1.0f - t) * p1 + t * p2;
        Vector3D a3 = (1.0f - t) * p2 + t * p3;
        Vector3D b1 = (1.0f - t) * a1 + t * a2;
        Vector3D b2 = (1.0f - t) * a2 + t * a3;
        Vector3D c1 = (1.0f - t) * b1 + t * b2;
        CubicBezier left{p0, a1, b1, c1};
        CubicBezier right{c1, b2, a3, p3};
        return {left, right};
    }
};

// Bounding box struct
struct BBox {
    float minX = std::numeric_limits<float>::infinity();
    float maxX = -std::numeric_limits<float>::infinity();
    float minY = std::numeric_limits<float>::infinity();
    float maxY = -std::numeric_limits<float>::infinity();

    bool overlaps(const BBox& other) const {
        return !(maxX < other.minX || other.maxX < minX || maxY < other.minY || other.maxY < minY);
    }
};

// Compute bounding box for a cubic Bézier (analytic, no sampling)
BBox getBBox(const CubicBezier& bez) {
    BBox bbox;
    auto addPoint = [&](const Vector3D& pt) {
        bbox.minX = std::min(bbox.minX, pt.x());
        bbox.maxX = std::max(bbox.maxX, pt.x());
        bbox.minY = std::min(bbox.minY, pt.y());
        bbox.maxY = std::max(bbox.maxY, pt.y());
    };

    addPoint(bez.p0);
    addPoint(bez.p3);

    // Solve for extrema (roots of derivative for x and y)
    auto solveQuadratic = [](float a, float b, float c, std::vector<float>& roots) {
        if (std::abs(a) < 1e-6f) {
            if (std::abs(b) > 1e-6f) roots.push_back(-c / b);
            return;
        }
        float disc = b * b - 4.0f * a * c;
        if (disc < 0.0f) return;
        float sqrtDisc = std::sqrt(disc);
        roots.push_back((-b - sqrtDisc) / (2.0f * a));
        roots.push_back((-b + sqrtDisc) / (2.0f * a));
    };

    // Derivative coefficients
    float ax = 3.0f * (bez.p1.x() - bez.p0.x());
    float bx = 3.0f * (bez.p2.x() - bez.p1.x()) - ax;
    float cx = 3.0f * (bez.p3.x() - bez.p2.x()) - 2.0f * bx - ax;  // Wait, correct expansion
    // Actually, a = 3*(p3.x - 3*p2.x + 3*p1.x - p0.x), b = 6*(p0.x - 2*p1.x + p2.x), c = 3*(p1.x - p0.x)
    float a_x = 3.0f * (bez.p3.x() - 3.0f * bez.p2.x() + 3.0f * bez.p1.x() - bez.p0.x());
    float b_x = 6.0f * (bez.p0.x() - 2.0f * bez.p1.x() + bez.p2.x());
    float c_x = 3.0f * (bez.p1.x() - bez.p0.x());
    std::vector<float> roots_x;
    solveQuadratic(a_x, b_x, c_x, roots_x);

    float a_y = 3.0f * (bez.p3.y() - 3.0f * bez.p2.y() + 3.0f * bez.p1.y() - bez.p0.y());
    float b_y = 6.0f * (bez.p0.y() - 2.0f * bez.p1.y() + bez.p2.y());
    float c_y = 3.0f * (bez.p1.y() - bez.p0.y());
    std::vector<float> roots_y;
    solveQuadratic(a_y, b_y, c_y, roots_y);

    for (float rt : roots_x) {
        if (rt > 0.0f && rt < 1.0f) addPoint(bez.evaluate(rt));
    }
    for (float rt : roots_y) {
        if (rt > 0.0f && rt < 1.0f) addPoint(bez.evaluate(rt));
    }
    return bbox;
}

// SubCurve for tracking parameter range
struct SubCurve {
    CubicBezier curve;
    float t_min, t_max;
};

// Refine intersection using Newton-Raphson
std::pair<float, float> refineIntersection(const CubicBezier& c1, const CubicBezier& c2, float t0, float s0, int max_iter = 10, float tol = 1e-6f) {
    float t = t0, s = s0;
    for (int i = 0; i < max_iter; ++i) {
        Vector3D p1 = c1.evaluate(t);
        Vector3D p2 = c2.evaluate(s);
        Vector3D f = p1 - p2;
        if (f.lengthSquared() < tol) break;
        Vector3D d1 = c1.derivative(t);
        Vector3D d2 = c2.derivative(s);
        float j11 = d1.x(), j12 = -d2.x();
        float j21 = d1.y(), j22 = -d2.y();
        float det = j11 * j22 - j12 * j21;
        if (std::abs(det) < 1e-6f) break;
        float dx = f.x(), dy = f.y();
        float dt = (j22 * dx - j12 * dy) / det;
        float ds = (j11 * dy - j21 * dx) / det;
        t -= dt;
        s -= ds;
        t = std::clamp(t, 0.0f, 1.0f);
        s = std::clamp(s, 0.0f, 1.0f);
    }
    return {t, s};
}

// Find intersections between two single cubic segments
std::vector<std::pair<float, float>> findIntersections(const CubicBezier& c1, const CubicBezier& c2, float threshold = 1e-3f, int max_depth = 20) {
    std::vector<std::pair<float, float>> results;
    std::function<void(const SubCurve&, const SubCurve&, int)> rec = [&](const SubCurve& sc1, const SubCurve& sc2, int depth) {
        BBox bb1 = getBBox(sc1.curve);
        BBox bb2 = getBBox(sc2.curve);
        if (!bb1.overlaps(bb2)) return;
        float len1 = sc1.t_max - sc1.t_min;
        float len2 = sc2.t_max - sc2.t_min;
        if (depth > max_depth || (len1 < threshold && len2 < threshold)) {
            float t1 = (sc1.t_min + sc1.t_max) / 2.0f;
            float t2 = (sc2.t_min + sc2.t_max) / 2.0f;
            results.emplace_back(t1, t2);
            return;
        }
        float mid1 = (sc1.t_min + sc1.t_max) / 2.0f;
        auto [l1, r1] = sc1.curve.split(0.5f);
        SubCurve sl1{l1, sc1.t_min, mid1};
        SubCurve sr1{r1, mid1, sc1.t_max};
        float mid2 = (sc2.t_min + sc2.t_max) / 2.0f;
        auto [l2, r2] = sc2.curve.split(0.5f);
        SubCurve sl2{l2, sc2.t_min, mid2};
        SubCurve sr2{r2, mid2, sc2.t_max};
        rec(sl1, sl2, depth + 1);
        rec(sl1, sr2, depth + 1);
        rec(sr1, sl2, depth + 1);
        rec(sr1, sr2, depth + 1);
    };
    SubCurve init1{c1, 0.0f, 1.0f};
    SubCurve init2{c2, 0.0f, 1.0f};
    rec(init1, init2, 0);
    return results;
}

// Main function to section the curve at intersection
bool sectionCurveAtIntersection(const BezierCurve& curve, const BezierCurve& other, std::pair<BezierCurve, BezierCurve>& sectioned, IntersectionInfo& info) {
    size_t num_segs_curve = curve.size() - 1;
    size_t num_segs_other = other.size() - 1;
    if (num_segs_curve == 0 || num_segs_other == 0) return false;

    float min_global_t = std::numeric_limits<float>::infinity();
    Vector3D inter_point;
    float selected_local_t = -1.0f;
    size_t selected_seg = -1;

    for (size_t seg1 = 0; seg1 < num_segs_curve; ++seg1) {
        const BezierControl& h1 = curve[seg1];
        const BezierControl& h2 = curve[seg1 + 1];
        CubicBezier cb1{h1.point, h1.point + h1.rightControl, h2.point + h2.leftControl, h2.point};

        for (size_t seg2 = 0; seg2 < num_segs_other; ++seg2) {
            const BezierControl& oh1 = other[seg2];
            const BezierControl& oh2 = other[seg2 + 1];
            CubicBezier cb2{oh1.point, oh1.point + oh1.rightControl, oh2.point + oh2.leftControl, oh2.point};

            auto rough_inters = findIntersections(cb1, cb2);
            for (auto& rs : rough_inters) {
                auto [t_local, s_local] = refineIntersection(cb1, cb2, rs.first, rs.second);
                Vector3D p = cb1.evaluate(t_local);
                Vector3D q = cb2.evaluate(s_local);
                if ((p - q).lengthSquared() < 1e-6f) {
                    float global_t = static_cast<float>(seg1) / static_cast<float>(num_segs_curve) + t_local / static_cast<float>(num_segs_curve);
                    if (global_t < min_global_t) {
                        min_global_t = global_t;
                        inter_point = p;
                        selected_local_t = t_local;
                        selected_seg = seg1;
                    }
                }
            }
        }
    }

    if (selected_local_t < 0.0f) return false;

    // Now split at selected_seg and selected_local_t
    BezierCurve first, second;
    first.setStrokeProperties(curve.getStrokeProperties());
    second.setStrokeProperties(curve.getStrokeProperties());

    // Add handles up to the split segment
    for (size_t i = 0; i < selected_seg; ++i) {
        first += curve[i];
    }

    // Get the split segment
    const BezierControl& split_h1 = curve[selected_seg];
    const BezierControl& split_h2 = curve[selected_seg + 1];
    CubicBezier split_cb{split_h1.point, split_h1.point + split_h1.rightControl, split_h2.point + split_h2.leftControl, split_h2.point};

    // Split controls
    auto [left_cb, right_cb] = split_cb.split(selected_local_t);

    // Add modified start handle for first
    Vector3D new_right = left_cb.p1 - left_cb.p0;  // scaled rightControl
    first += BezierControl(split_h1.point, split_h1.leftControl, new_right);

    // Add new end handle for first
    Vector3D new_left_first = left_cb.p2 - left_cb.p3;
    first += BezierControl(left_cb.p3, new_left_first, Vector3D(0.0f, 0.0f, 0.0f));  // rightControl unused

    // Start second with new start handle
    Vector3D new_right_second = right_cb.p1 - right_cb.p0;
    second += BezierControl(right_cb.p0, Vector3D(0.0f, 0.0f, 0.0f), new_right_second);  // leftControl unused

    // Add modified end handle for second
    Vector3D new_left_second = right_cb.p2 - right_cb.p3;
    second += BezierControl(split_h2.point, new_left_second, split_h2.rightControl);

    // Add remaining handles to second
    for (size_t i = selected_seg + 2; i < curve.size(); ++i) {
        second += curve[i];
    }

    sectioned = {first, second};
    info.point = inter_point;
    info.t = min_global_t;
    return true;
}

bool invert2x2(const Matrix2x2& m, Matrix2x2& inv) {
    double det = m.a * m.d - m.b * m.c;
    if (std::abs(det) < 1e-6) return false;
    inv.a = m.d / det;
    inv.b = -m.b / det;
    inv.c = -m.c / det;
    inv.d = m.a / det;
    return true;
}

Vector3D solve2x2(const Matrix2x2& A, double bx, double by) {
    Matrix2x2 inv;
    if (!invert2x2(A, inv)) return {0, 0, 0};
    double cx = inv.a * bx + inv.b * by;
    double cy = inv.c * bx + inv.d * by;
    return {(float)cx, (float)cy, 0};  // Assuming 2D
}

double closestParam(const GameFusion::BezierCurve& curve, const QPointF& Q, double initial_t) {
    if (curve.size() < 2) return 0.5;
    const Vector3D& P0 = curve[0].point;
    const Vector3D C1 = curve[0].point + curve[0].rightControl;
    const Vector3D C2 = curve[1].point + curve[1].leftControl;
    const Vector3D& P3 = curve[1].point;

    double t = initial_t;
    for (int iter = 0; iter < 5; ++iter) {
        double u = 1 - t;
        Vector3D B = u * u * u * P0 + 3 * u * u * t * C1 + 3 * u * t * t * C2 + t * t * t * P3;
        Vector3D dB = 3 * u * u * (C1 - P0) + 6 * u * t * (C2 - C1) + 3 * t * t * (P3 - C2);
        Vector3D ddB = 6 * u * (C2 - 2 * C1 + P0) + 6 * t * (P3 - 2 * C2 + C1);

        Vector3D diff(B.x() - Q.x(), B.y() - Q.y(), 0);
        double f = Vector3D::dotProduct(diff, dB);  // Changed from diff.dot(dB)
        double df = Vector3D::dotProduct(dB, dB) + Vector3D::dotProduct(diff, ddB);
        if (std::abs(df) < 1e-6) break;
        t -= f / df;
        t = std::clamp(t, 0.0, 1.0);
    }
    return t;
}

std::vector<double> chordParams(const std::vector<StrokePoint>& points) {
    std::vector<double> t(points.size(), 0.0);
    double total_len = 0.0;
    for (size_t i = 1; i < points.size(); ++i) {
        QPointF diff = points[i].pos - points[i-1].pos;
        total_len += std::sqrt(diff.x()*diff.x() + diff.y()*diff.y());
        t[i] = total_len;
    }
    if (total_len > 0) {
        for (double& val : t) val /= total_len;
    }
    return t;
}

// Function to simplify a BezierCurve by merging adjacent segments where possible,
// based on the geometric control points, without relying on dense sampling.
// The threshold represents the maximum allowed deviation (distance) of the original
// middle anchor point from the approximated merged segment.
// If the deviation is <= threshold, the segments are merged, preserving end tangents.
// This automatically preserves sharp corners, as they would cause high deviation.
// Reasoning: Merging two segments removes the middle anchor and approximates with
// a single cubic using the original start tangent of the first segment and end
// tangent of the second segment. The deviation check ensures visual equivalence.
// The process is a greedy left-to-right pass, which may not find the global optimum
// but is efficient and simple.

BezierCurve simplifyBezierCurve(const BezierCurve& curve, double threshold) {
    size_t num_handles = curve.size();
    if (num_handles < 3) {
        // Nothing to simplify (fewer than 2 segments)
        return curve;
    }

    std::vector<BezierControl> new_handles;
    std::vector<float> new_pressures;
    const auto& orig_pressures = curve.strokePressure();

    bool has_pressures = !orig_pressures.empty() && orig_pressures.size() == num_handles;
    if (has_pressures) {
        new_pressures.push_back(orig_pressures[0]);
    }

    new_handles.push_back(curve[0]);

    size_t current = 0;
    while (current + 1 < num_handles) {
        if (current + 2 < num_handles) {
            // Attempt to merge segments current to current+1 and current+1 to current+2
            Vector3D P0 = curve[current].point;
            Vector3D L0 = curve[current].leftControl; // Preserve for previous segment
            Vector3D R0 = curve[current].rightControl; // Will adjust magnitude
            Vector3D P1 = curve[current + 1].point;
            Vector3D L1 = curve[current + 1].leftControl;
            Vector3D R1 = curve[current + 1].rightControl;
            Vector3D P3 = curve[current + 2].point;
            Vector3D L2 = curve[current + 2].leftControl; // Will adjust magnitude
            Vector3D R2 = curve[current + 2].rightControl; // Preserve for next segment

            // Create temporary single-segment curve for deviation check
            BezierControl start_merged(P0, L0, R0);
            BezierControl end_merged(P3, L2, R2);
            BezierCurve merged;
            merged += start_merged;
            merged += end_merged;

            // Compute deviation: distance from original middle point to closest point on merged curve
            QPointF middle_q(P1.x(), P1.y());
            double t_m = closestParam(merged, middle_q, 0.5);
            Vector3D proj = merged.evaluate(t_m);
            double dist = (proj - P1).length();

            if (dist <= threshold) {
                // Adjust tangent magnitudes parametrically
                // Use original tangent directions but scale magnitudes
                //Vector3D C1_orig = P0 + R0; // Original first control point
                //Vector3D C2_orig = P3 + L2; // Original second control point
                //Vector3D C1_mid = P1 + L1;  // Control point from first segment
                //Vector3D C2_mid = P1 + R1;  // Control point from second segment



                // Solve for C1 and C2 such that B(t_m) ≈ P1
                // B(t_m) = u^3 P0 + 3u^2 t C1 + 3u t^2 C2 + t^3 P3
                // We want C1 and C2 to lie on original tangent directions
                // R0_new = alpha * R0, L2_new = beta * L2
                // Compute scalars alpha and beta to minimize error at P1
                // Compute coefficients
                double u = 1.0 - t_m;
                double u2 = u * u;
                double u3 = u2 * u;
                double t2 = t_m * t_m;
                double t3 = t2 * t_m;
                double coeff1 = 3.0 * u2 * t_m;  // For C1
                double coeff2 = 3.0 * u * t2;     // For C2

                // Compute rhs = P1 - u3 P0 - t3 P3
                Vector3D rhs = P1 - (u3 * P0 + t3 * P3);

                // Compute b = rhs - coeff1 * P0 - coeff2 * P3
                Vector3D b = rhs - (coeff1 * P0 + coeff2 * P3);

                // Get unit vectors (handle zero length)
                Vector3D unit_R0(0,0,0), unit_L2(0,0,0);
                float orig_len_R0 = R0.length();
                float orig_len_L2 = L2.length();
                if (orig_len_R0 > 1e-6f) {
                    unit_R0 = R0 / orig_len_R0;
                }
                if (orig_len_L2 > 1e-6f) {
                    unit_L2 = L2 / orig_len_L2;
                }

                // Set up 2x2 matrix A: columns are coeff1 * unit_R0 and coeff2 * unit_L2
                Matrix2x2 A;
                A.a = coeff1 * unit_R0.x();
                A.b = coeff2 * unit_L2.x();
                A.c = coeff1 * unit_R0.y();
                A.d = coeff2 * unit_L2.y();

                // Solve for alpha, beta
                Vector3D solution = solve2x2(A, b.x(), b.y());
                double alpha = solution.x();
                double beta = solution.y();

                // Fallback if solve fails (singular matrix, e.g., parallel tangents)
                Matrix2x2 inv;
                if (!invert2x2(A, inv)) {
                    // Fallback: average original lengths
                    float avg_len = (orig_len_R0 + orig_len_L2) * 0.5f;
                    if (orig_len_R0 > 0) R0 = unit_R0 * avg_len;
                    if (orig_len_L2 > 0) L2 = unit_L2 * avg_len;
                } else {
                    // Apply scales: new magnitude = alpha (or beta), since we used unit vectors
                    // But to preserve "feel", optionally scale by original length factor, but here we set directly
                    if (orig_len_R0 > 0) R0 = unit_R0 * alpha;
                    if (orig_len_L2 > 0) L2 = unit_L2 * beta;
                }

                // If you want to prevent direction flips, clamp alpha and beta to positive:
                // alpha = std::max(0.0, alpha);
                // beta = std::max(0.0, beta);
                // Then R0 = unit_R0 * alpha; etc.

                // Proceed with new_start and new_end as before

                // Create new merged segment with adjusted tangents
                BezierControl new_start(P0, L0, R0);
                BezierControl new_end(P3, L2, R2);
                new_handles.push_back(new_end);
                if (has_pressures) {
                    new_pressures.push_back(orig_pressures[current + 2]);
                }
                current += 2;
                continue;
            }
        }

        // Cannot merge or no more to merge: add the next original handle
        new_handles.push_back(curve[current + 1]);
        if (has_pressures) {
            new_pressures.push_back(orig_pressures[current + 1]);
        }
        current += 1;
    }

    // Build the simplified curve
    BezierCurve simplified;
    simplified.setStrokeProperties(curve.getStrokeProperties());
    for (const auto& handle : new_handles) {
        simplified += handle;
    }
    if (has_pressures) {
        simplified.setStrokePressure(new_pressures);
    }

    return simplified;
}

void BezierCurve::linearize(bool loop) {
    if (handles_.size() < 2) return; // No-op for <2 points

    m_isClosed = loop;

    for (int i = 0; i < handles_.size(); ++i){
        BezierControl &handle = handles_[i];
        handle.leftControl= Vector3D();
        handle.rightControl= Vector3D();
    }

    for (int i = 0; i < handles_.size(); ++i) {
        Vector3D prior = getAnchor(i - 1);
        Vector3D next = getAnchor(i + 1);

        // Incoming tangent: Along prior-to-current, length = local segment distance (uniform linear time per segment)
        Vector3D incoming = handles_[i].point - prior;
        float incomingMag = std::sqrt(Vector3D::dotProduct(incoming, incoming));
        if (incomingMag > 0) {
            handles_[i].leftControl = (incoming / incomingMag) * (incomingMag * 0.5f); // Half for endpoint control
        }

        // Outgoing tangent: Along current-to-next, length = local segment distance
        Vector3D outgoing = next - handles_[i].point;
        float outgoingMag = std::sqrt(Vector3D::dotProduct(outgoing, outgoing));
        if (outgoingMag > 0) {
            handles_[i].rightControl = (outgoing / outgoingMag) * (outgoingMag * 0.5f);
        }
    }
}

Vector3D BezierCurve::getAnchor(int index) const {
    if(handles_.empty())
        return Vector3D();

    if (m_isClosed) {
        index = (index + handles_.size()) % handles_.size();
    } else {
        index = index < 0 ? 0 : index;
        index = index >= handles_.size() ? handles_.size()-1 : index;
    }
    return handles_[index].point;
}

void BezierCurve::smoothAuto(bool loop)
{



    for(int i=0; i<handles_.size(); i++)
    {
        /*
                get prior, current and nex handles, for each current set the tangents
                */
        BezierControl *prior = i>0 ? &handles_[i-1] : nullptr;
        BezierControl &current   = handles_[i];
        BezierControl *next   = i+1 < handles_.size() ? &handles_[i+1] : nullptr;

        /*
        if(li.next())
            next   = &(**(li.next()));
        else if(loop)
            next = &(**(m_handleList.start()));

        if(li.prior())
            prior = &(**(li.prior()));
        else if(loop)
            prior =  &(**(m_handleList.last()));
        */



        Vector3D seg1;
        Vector3D seg2;

        Vector3D midpoint1, midpoint2;

        if(prior)
        {
            seg1 =  current.point - prior->point;
            //midpoint1 = prior->position() + seg1 * 0.5;
            midpoint1 = prior->point;
        }
        else
            midpoint1 = current.point;

        if(next)
        {
            seg2 = next->point - current.point;
            //midpoint2 = current.position() +  seg2 * 0.5;
            midpoint2 = next->point;
        }
        else
            midpoint2 = current.point;





        //float tangentMag = tangentEdge.magnitude();

        // compute proportion factor for left and right tangents
        float seg1Mag = seg1.magnitude();
        float seg2Mag = seg2.magnitude();



        //float totalMag = seg1Mag + seg2Mag;
        /*
                float factor1 = seg1Mag / totalMag;
                float factor2 = seg2Mag / totalMag;
                */



        seg1.normalize();
        seg2.normalize();


        //
        // New smppth model compute tanget normal based on the angle
        if(prior && next)
        {
            if( seg1Mag < seg2Mag)
            {
                midpoint2 = current.point + seg2 * seg1Mag;
                seg2Mag = seg1Mag;
            }
            else if(seg1Mag > seg2Mag)
            {
                midpoint1 = current.point - seg1 * seg2Mag;
                seg1Mag = seg2Mag;
            }

        }
        //

        Vector3D tangentEdge = midpoint2 - midpoint1;
        Vector3D tangentNormal = Vector3D::normal(tangentEdge);

        /*
                float dot1 = Vector3D::dotProduct(seg1, seg2);

                //Log::info("dot %f\n", dot1);
                float factor1 = 0.5 * (1. - SmoothPath.getValue(dot1));
                float factor2 = 0.5 * (1. - SmoothPath.getValue(dot1));

                current.leftTangent()  = -tangentNormal * (factor1 * seg1Mag);
                current.rightTangent() =  tangentNormal * (factor2 * seg2Mag);
                */
        current.leftControl = -tangentNormal * (seg1Mag*0.5f);
        current.rightControl =  tangentNormal * (seg2Mag*0.5f);




    }

    //print();
    //exit(0);

    //smoothTangents(normal);
}

void BezierCurve::smooth2(float smoothIn, float smoothOut)
{
    //ListIterator <BezierHandle> li(m_handleList);

    for(int i=1; i<handles_.size()-1; i++)
    //while(!li.end())
    {
        //if(li.prior())
        {
            //BezierControl prior = 0;
            BezierControl &current   = handles_[i];
            //BezierControl *next   = 0;

            //if(li.next())
                BezierControl &next   = handles_[i+1];
            //else
            //    next = &(**(m_handleList.start()));

            //if(li.prior())
                BezierControl &prior = handles_[i-1];
           // else
            //    prior =  &(**(m_handleList.last()));


            /*************/

            Vector3D vdistance = current.point - prior.point;
            Vector3D vhalfD    = vdistance * 0.5;

            prior.leftControl = vhalfD * smoothIn;
            current.rightControl = -vhalfD * smoothOut;

            //continue;

            /**********/

            /*

            float distance = handle.position().x() - prior.position().x();
            float halfD = distance * 0.5;
            prior.rightTangent()[0] = -halfD * smoothIn;
            handle.leftTangent().x() = halfD * smoothOut;

            float distanceZ = handle.position().z() - prior.position().z();
            float halfDZ = distanceZ * 0.5;
            prior.rightTangent().z() = halfDZ * smoothIn;
            handle.leftTangent().z() = -halfDZ * smoothOut;
                */
        }
        //li++;
    }

    //smoothTangents(normal);
}

void BezierCurve::smooth(float strength, bool loop) {
    if (handles_.size() < 2) return; // No-op for <2 points

    m_isClosed = loop;

    for (int i = 0; i < handles_.size(); ++i){
        BezierControl &handle = handles_[i];
        handle.leftControl= Vector3D();
        handle.rightControl= Vector3D();
    }

    for (int i = 0; i < handles_.size(); ++i) {
        Vector3D prior = getAnchor(i - 1);
        Vector3D current = handles_[i].point;
        Vector3D next = getAnchor(i + 1);

        handles_[i].leftControl = Vector3D::distance(prior, current) * 0.5 * Vector3D(-1,0,0) - prior;
        handles_[i].rightControl = Vector3D();

        return;

        // Segment vectors
        Vector3D seg1 = current - prior;
        Vector3D seg2 = next - current;

        // Magnitudes for local proportional scaling (uniform time per segment)
        float seg1Mag = std::sqrt(Vector3D::dotProduct(seg1, seg1));
        float seg2Mag = std::sqrt(Vector3D::dotProduct(seg2, seg2));

        // Adjust midpoints for unequal segments (inspired by your smoothAuto for symmetry)
        Vector3D midpoint1 = prior;
        Vector3D midpoint2 = next;
        if (seg1Mag > 0 && seg2Mag > 0) {
            if (seg1Mag < seg2Mag) {
                midpoint2 = current + (seg2 / seg2Mag) * seg1Mag;
                seg2Mag = seg1Mag;
            } else if (seg1Mag > seg2Mag) {
                midpoint1 = current - (seg1 / seg1Mag) * seg2Mag;
                seg1Mag = seg2Mag;
            }
        }

        // Tangent edge and perpendicular normal (direction for smoothness)
        Vector3D tangentEdge = midpoint2 - midpoint1;
        float tangentEdgeMag = std::sqrt(Vector3D::dotProduct(tangentEdge, tangentEdge));
        Vector3D tangentNormal;
        if (tangentEdgeMag > 0) {
            tangentNormal = perpendicular(tangentEdge / tangentEdgeMag); // Normalize direction
        } else {
            tangentNormal = Vector3D(0, 1); // Fallback (arbitrary perpendicular)
        }

        // Set symmetric smooth tangents: Per-segment scaling for equal time pace
        // Length: local segment mag * strength * 0.5 (half for in/out balance)
        handles_[i].leftControl = -tangentNormal * (seg1Mag * strength * 0.5f);
        handles_[i].rightControl = tangentNormal * (seg2Mag * strength * 0.5f);
    }
}

} // namespace GameFusion
