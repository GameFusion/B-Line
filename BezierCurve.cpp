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

} // namespace GameFusion
