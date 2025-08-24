// Andreas Carlen
// June 12, 2025
//
// **Test Logging**
// Unit test source, C++ Version, prototype for logging prompts and results to a server.
// This test will create a PromptLogger instance, configure it to point to a local server,
// and simulate logging a prompt and its result. The test will verify that the log entry
// is correctly formatted and sent to the server.

#include "PromtLogger.h"
#include "ScriptBreakdown.h"
#include "MainWindow.h"

/******
Expected JSON content:

Verify JSON log content for "test command":
     json
     {
         "type": "console",
         "timestamp": "2025-09-20T20:55:00Z",
         "user": "default_user",
         "version": "1.0",
         "llm": "llama",
         "prompt": "test command",
         "response": "Sample response",
         "tokens": 2,
         "cost": 0.0,
         "context": {
             "response": "Sample response",
             "contextInfo": "Context: session=1",
             "model": "model.gguf",
             "vendor": "Llama",
             "location": "local",
             "apiTech": "LlamaEngine",
             "processingTime": 0.001
         }
     }

******/



int main() {
	PromptLogger* logger = new PromptLogger();
        logger->setServerConfig("http://localhost:50000", "test_token");
        ScriptBreakdown breakdown("test_script", 24.0f, nullptr, nullptr, new LlamaClient(), logger);
        breakdown.breakdownScript(); // Triggers processShots, processSequences, processActs
        MainWindow window;
        window.consoleCommand("test command");
        return 0;
}

