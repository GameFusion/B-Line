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
    QJsonArray vertexArray;
    for (const auto& v : vertices_) {
        QJsonObject point;
        point["x"] = v.x();
        point["y"] = v.y();
        vertexArray.append(point);
    }
    json["vertices"] = vertexArray;

    QJsonArray handleArray;
    for (const auto& h : handles_) {
        QJsonObject handleJson;
        handleJson["pointX"] = h.point.x();
        handleJson["pointY"] = h.point.y();
        handleJson["leftX"] = h.leftControl.x();
        handleJson["leftY"] = h.leftControl.y();
        handleJson["rightX"] = h.rightControl.x();
        handleJson["rightY"] = h.rightControl.y();
        handleArray.append(handleJson);
    }
    json["handles"] = handleArray;
}

void BezierCurve::fromJson(const QJsonObject& json) {
    vertices_.clear();
    handles_.clear();

    QJsonArray vertexArray = json["vertices"].toArray();
    for (const auto& point : vertexArray) {
        QJsonObject pt = point.toObject();
        vertices_.emplace_back(pt["x"].toDouble(), pt["y"].toDouble(), 0.0f);
    }

    QJsonArray handleArray = json["handles"].toArray();
    for (const auto& handleJson : handleArray) {
        QJsonObject h = handleJson.toObject();
        Vector3D point(h["pointX"].toDouble(), h["pointY"].toDouble(), 0.0f);
        Vector3D left(h["leftX"].toDouble(), h["leftY"].toDouble(), 0.0f);
        Vector3D right(h["rightX"].toDouble(), h["rightY"].toDouble(), 0.0f);
        handles_.emplace_back(point, left, right);
    }
}

} // namespace GameFusion
