#ifndef PROJECTCONTEXT_H
#define PROJECTCONTEXT_H

#include <QString>

/*
 *
 * Usage

// In MainWindow constructor or initialization method
void MainWindow::initializeProject() {
    ProjectContext::instance().setCurrentProjectPath("D:/GameFusion/Projects/MyProject"); // Example path
    ProjectContext::instance().setCurrentProjectName("MyProject"); // Example name
    // ... other initialization
}


ProjectContext::instance().currentProjectPath()
ProjectContext::instance().currentProjectName()
 */

class ProjectContext {
public:
    static ProjectContext& instance() {
        static ProjectContext instance;
        return instance;
    }

    // Delete copy constructor and assignment operator to prevent copying
    ProjectContext(const ProjectContext&) = delete;
    ProjectContext& operator=(const ProjectContext&) = delete;

    void setCurrentProjectPath(const QString& path) { m_currentProjectPath = path; }
    QString currentProjectPath() const { return m_currentProjectPath; }

    void setCurrentProjectName(const QString& name) { m_currentProjectName = name; }
    QString currentProjectName() const { return m_currentProjectName; }

private:
    ProjectContext() = default; // Private constructor for singleton
    QString m_currentProjectPath;
    QString m_currentProjectName;
};

#endif // PROJECTCONTEXT_H
