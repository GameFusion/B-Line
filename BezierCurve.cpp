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

} // namespace GameFusion
