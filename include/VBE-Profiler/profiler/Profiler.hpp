#ifndef PROFILER_HPP
#define PROFILER_HPP
#include <VBE-Profiler/profiler/imgui.h>
#include <VBE/VBE.hpp>
#include <VBE-Scenegraph/VBE-Scenegraph.hpp>

#define PROFILER_HIST_SIZE 50

class DeferredContainer;
class Profiler : public GameObject {
    public:
        Profiler();
        Profiler(std::string vertShader, std::string fragShader);
        ~Profiler();

        static void pushMark(const std::string& name, const std::string& definition);
        static void popMark();
        static bool isShown();
        static void setShown(bool shown);
        static bool isLogShown();
        static void setShowLog(bool shown);
        static bool isTimeShown();
        static void setShowTime(bool shown);

    protected:
        virtual void render(const ImDrawData* data) const;
        virtual void renderCustomInterface() const;

    private:
        struct Node {
                Node() {start();}
                Node(const std::string& name, const std::string& desc, Node* parent)
                    : parent(parent), name(name), desc(desc) {start();}
                ~Node() {}

                float getTime() const {return totalTime;}
                void start() {timeStart = Clock::getSeconds();}
                void stop() {timeEnd = Clock::getSeconds(); totalTime += timeEnd-timeStart; timeStart = 0.0f;}

                Node* parent = nullptr;
                std::list<Node> children;
                std::string name = std::string("invalid");
                std::string desc = std::string("invalid");
            private:
                float totalTime = 0.0f;
                float timeStart = 0.0f;
                float timeEnd = 0.0f;
        };

        class Watcher final : public GameObject {
            public:
                Watcher();
                ~Watcher();

                void fixedUpdate(float deltaTime);
                void update(float deltaTime);
                void draw() const;
        };

        struct Historial final {
                Historial(unsigned long int id) : id(id) {}
                const unsigned long int id = 0;
                float current = 0.0f;
                float past[PROFILER_HIST_SIZE];
        };

        static void renderHandle(ImDrawData* data);
        static const char* getClipHandle(void* user_data);
        static void setClipHandle(void* user_data, const char* text);

        //callback
        const char* getClip() const;
        //callback
        void setClip(const char* text) const;

        void fixedUpdate(float deltaTime) final override;
        void update(float deltaTime) final override;
        void draw() const final override;
        void processNodeAverage(const Node& n);
        void resetTreeDraw() const;
        void resetTreeUpdate() const;
        void resetTreeFixed() const;
        void resetTreeSwap() const;
        void resetTreeWhole() const;
        void setImguiIO(float deltaTime) const;
        void timeWindow() const;
        void logWindow() const;
        void uiProcessNode(const Node& n) const;

        static Profiler* instance;
        static std::string defaultVS;
        static std::string defaultFS;

        int timeAvgOffset = -1;
        mutable int frameCount = 0;
        mutable float timePassed = 0.0f;
        mutable int FPS = 0;
        bool showProfiler = false;
        bool showTime = true;
        bool showLog = true;
        float sampleRate = 0.5f;
        float windowAlpha = 0.9f;
        vec2ui wsize = vec2ui(0,0);
        mutable Node treeUpdate;
        mutable Node treeDraw;
        mutable Node treeFixed;
        mutable Node treeSwap;
        mutable Node treeWhole;
        mutable Node* currentNode = nullptr;
        std::map<std::string, Historial> hist;
        mutable MeshIndexed model;
        Texture2D tex;
        ShaderProgram program;
        mutable std::string clip = "";
};

#endif // PROFILER_HPP
