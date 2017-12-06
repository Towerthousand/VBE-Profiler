#include <VBE-Profiler/profiler/Profiler.hpp>
#include <cstring>
#include <iomanip>

Profiler* Profiler::instance = nullptr;
std::string Profiler::defaultVS = " \
    #version 420\n\
    \
    uniform mat4 MVP;\
    \
    in vec2 a_position;\
    in vec2 a_texCoord;\
    in vec4 a_color; \
    \
    out vec2 vTexCoord;\
    out vec4 vColor;\
    \
    void main() { \
        vTexCoord = a_texCoord; \
        vColor = a_color; \
        gl_Position = MVP * vec4(vec3(a_position, 0.0f), 1.0); \
    }";

std::string Profiler::defaultFS = " \
    #version 420 \n\
    uniform sampler2D fontTex;\
    \
    in vec2 vTexCoord;\
    in vec4 vColor;\
    out vec4 finalColor;\
    \
    void main(void) { \
        finalColor = vec4(texture(fontTex,vTexCoord)*vColor); \
    }";

namespace {
    std::string toString(float f, int width, int precision, bool left) {
        std::ostringstream temp;
        if(left) temp << std::left;
        temp << std::fixed << std::setprecision(precision) << std::setw(width) << f;
        return temp.str();
    }
}

Profiler::Profiler() : Profiler(defaultVS, defaultFS) {
}

Profiler::Profiler(std::string vertShader, std::string fragShader) {
    //setup singleton
    VBE_ASSERT(instance == nullptr, "Created two profilers");
    instance = this;

    // Pick program
    program = ShaderProgram(vertShader, fragShader);

    //will update and draw last of all
    setUpdatePriority(1000);
    setDrawPriority(1000);

    //setup UI model
    std::vector<Vertex::Attribute> elems = {
        Vertex::Attribute("a_position", Vertex::Attribute::Float, 2),
        Vertex::Attribute("a_texCoord", Vertex::Attribute::Float, 2),
        Vertex::Attribute("a_color", Vertex::Attribute::UnsignedByte, 4, Vertex::Attribute::ConvertToFloatNormalized)
    };

    Vertex::Format format(elems);
    model = MeshIndexed(format, MeshIndexed::STREAM);

    //setup ui
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize.x = Window::getInstance()->getSize().x;
    io.DisplaySize.y = Window::getInstance()->getSize().y;
    io.IniFilename = "imgui.ini";
    io.RenderDrawListsFn = &Profiler::renderHandle;
    io.SetClipboardTextFn = &Profiler::setClipHandle;
    io.GetClipboardTextFn = &Profiler::getClipHandle;
    io.IniSavingRate = -1.0f; //disable ini
    io.ClipboardUserData = NULL;

    // Build texture atlas
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    tex = Texture2D(vec2ui(width, height), TextureFormat::RGBA);
    tex.setFilter(GL_LINEAR, GL_LINEAR);
    tex.setData(pixels, TextureFormat::RGBA, TextureFormat::UNSIGNED_BYTE);
    //glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    io.Fonts->TexID = (void *)(intptr_t)tex.getHandle();

    //add watcher
    Watcher* w = new Watcher();
    w->addTo(this);
}

Profiler::~Profiler() {
    ImGui::Shutdown();
    instance = nullptr;
}

//static
void Profiler::pushMark(const std::string& name, const std::string& definition) {
    VBE_ASSERT(instance != nullptr, "Null profiler");
    VBE_ASSERT(instance->currentNode != nullptr, "Popped main node on profiler");
    for(Node& child : instance->currentNode->children) {
        if(child.name == name) {
            child.start();
            instance->currentNode = &child;
            return;
        }
    }
    instance->currentNode->children.push_back(Node(name,definition, instance->currentNode));
    instance->currentNode = &instance->currentNode->children.back();
}

//static
void Profiler::popMark() {
    VBE_ASSERT(instance != nullptr, "Null profiler");
    VBE_ASSERT(instance->currentNode != nullptr, "Too many popped nodes on profiler");
    instance->currentNode->stop();
    instance->currentNode = instance->currentNode->parent;
}

//static
void Profiler::setShown(bool shown) {
    VBE_ASSERT(instance != nullptr, "Null profiler");
    instance->showProfiler = shown;
}

//static
bool Profiler::isLogShown() {
    return (isShown() && instance->showLog);
}

//static
void Profiler::setShowLog(bool shown) {
    VBE_ASSERT(instance != nullptr, "Null profiler");
    instance->showLog = shown;
}

//static
bool Profiler::isTimeShown() {
    return (isShown() && instance->showTime);
}

//static
void Profiler::setShowTime(bool shown) {
    VBE_ASSERT(instance != nullptr, "Null profiler");
    instance->showTime = shown;
}

//static
bool Profiler::isShown() {
    return (instance != nullptr && instance->showProfiler);
}

void Profiler::renderHandle(ImDrawData* data) {
    instance->render(data);
}

const char* Profiler::getClipHandle(void* user_data) {
    (void) user_data;
    return instance->getClip();
}

void Profiler::setClipHandle(void* user_data, const char* text) {
    (void) user_data;
    instance->setClip(text);
}

void Profiler::render(const ImDrawData* drawData) const {
    if (drawData->CmdListsCount == 0)
        return;

    GL_ASSERT(glDisable(GL_CULL_FACE));
    GL_ASSERT(glDepthFunc(GL_ALWAYS));
    GL_ASSERT(glEnable(GL_SCISSOR_TEST));

    // Setup orthographic projection matrix
    const float width = ImGui::GetIO().DisplaySize.x;
    const float height = ImGui::GetIO().DisplaySize.y;
    mat4f perspective = glm::ortho(0.0f, width, height, 0.0f, -1.0f, +1.0f);
    program.uniform("MVP")->set(perspective);

    // Set texture for font
    program.uniform("fontTex")->set(&tex);

    // Render command lists
    for (int n = 0; n < drawData->CmdListsCount; n++) {
        const ImDrawList* cmd_list = drawData->CmdLists[n];
        unsigned int idx_buffer_offset = 0;
        model.setVertexData((const unsigned char*)cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.size());
        model.setIndexData(cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size);

        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            GL_ASSERT(glScissor((int)pcmd->ClipRect.x, (int)(height - pcmd->ClipRect.w), (int)(pcmd->ClipRect.z - pcmd->ClipRect.x), (int)(pcmd->ClipRect.w - pcmd->ClipRect.y)));
            model.draw(program, idx_buffer_offset, pcmd->ElemCount);
            idx_buffer_offset += pcmd->ElemCount;
        }
    }
    GL_ASSERT(glDisable(GL_SCISSOR_TEST));
    GL_ASSERT(glDepthFunc(GL_LEQUAL));
    GL_ASSERT(glEnable(GL_CULL_FACE));
}

void Profiler::renderCustomInterface() const {
}

const char* Profiler::getClip() const {
    //TODO
    return "";
}

void Profiler::setClip(const char* text) const {
    (void) text; //TODO
}

void Profiler::fixedUpdate(float deltaTime) {
    (void) deltaTime;
    popMark(); //fixed update
}

void Profiler::update(float deltaTime) {
    popMark(); //update
    treeWhole.stop();
    processNodeAverage(treeSwap);
    processNodeAverage(treeDraw);
    processNodeAverage(treeFixed);
    processNodeAverage(treeUpdate);
    processNodeAverage(treeWhole);
    resetTreeWhole();
    pushMark("Profiler Prepare", "Time spent preparing the profiler geometry");
    if(timePassed >= sampleRate) {
        //update history
        timeAvgOffset = (timeAvgOffset + 1) % PROFILER_HIST_SIZE;
        for(auto it = hist.begin(); it != hist.end(); ++it) {
            it->second.past[timeAvgOffset] = (it->second.current/frameCount)*1000;
            it->second.current = 0.0f;
        }
        //update FPS
        timePassed -= sampleRate;
        FPS = float(frameCount)/sampleRate;
        frameCount = 0;
    }
    //do profiler
    if(Keyboard::justPressed(Keyboard::F1)) {
        showProfiler = !showProfiler;
        Mouse::setRelativeMode(!showProfiler);
    }
    setImguiIO(deltaTime);
    ImGui::NewFrame();
    if(showProfiler) {
        wsize = Window::getInstance()->getSize();
        ImGui::GetStyle().WindowRounding = 6;
        ImGui::GetStyle().FrameRounding = 6;
        if(showTime) timeWindow();
        if(showLog) logWindow();
        renderCustomInterface();
        //ImGui::ShowTestWindow();
    }
    //prepare for next frame
    frameCount++;
    timePassed += deltaTime;
    popMark(); //Profiler prepare
    resetTreeDraw();
}

void Profiler::draw() const {
    Profiler::pushMark("Profiler draw", "Time spent drawing the profiler UI");
    ImGui::Render();
    Profiler::popMark();
    popMark(); //draw
    resetTreeSwap();
}

void Profiler::processNodeAverage(const Profiler::Node& n) {
    if(hist.find(n.name) == hist.end()) {
        hist.insert(std::pair<std::string, Historial>(n.name, Historial(hist.size())));
        memset(hist.at(n.name).past, 0, sizeof(float)*PROFILER_HIST_SIZE);
    }
    hist.at(n.name).current += n.getTime();
    for(const Node& child : n.children)
        processNodeAverage(child);
}

void Profiler::resetTreeWhole() const {
    treeWhole = Node("Whole frame", "Time spent on the whole frame", nullptr);
    currentNode = &treeWhole;
}

void Profiler::resetTreeDraw() const {
    treeDraw = Node("Draw", "Time spent issuing GL commands and drawing stuff on the screen", nullptr);
    currentNode = &treeDraw;
}

void Profiler::resetTreeUpdate() const {
    treeUpdate = Node("Update", "Time spent updating variable game logic", nullptr);
    currentNode = &treeUpdate;
}

void Profiler::resetTreeFixed() const {
    treeFixed = Node("Fixed Update", "Time spent updating fixed game logic", nullptr);
    currentNode = &treeFixed;
}

void Profiler::resetTreeSwap() const {
    treeSwap = Node("Swap", "Time spent waiting for the GPU to finish all pending jobs", nullptr);
    currentNode = &treeSwap;
}

void Profiler::setImguiIO(float deltaTime) const {
    ImGuiIO& io = ImGui::GetIO();
    io.DeltaTime = deltaTime == 0.0f ? 0.00001f : deltaTime;
    io.MouseWheel = Mouse::wheelMovement().y != 0 ? (Mouse::wheelMovement().y > 0 ? 1 : -1) : 0;
    io.KeyCtrl = Keyboard::pressed(Keyboard::LControl);
    io.KeyShift = Keyboard::pressed(Keyboard::LShift);
    io.MouseDown[0] = Mouse::pressed(Mouse::Left);
    io.MousePos = vec2f(Mouse::position());
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(vec2f(Window::getInstance()->getSize()));
    io.UserData = nullptr;
    io.FontGlobalScale = 1.0f;
}

void Profiler::timeWindow() const {
    ImGui::Begin("Frame Times", nullptr, ImVec2(0.23f*wsize.x,0.55f*wsize.y), windowAlpha);
    ImGui::SetWindowPos(ImVec2(0.025f*wsize.x, 0.05f*wsize.y), ImGuiCond_FirstUseEver);
    ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.65f);
    ImGui::Text("With V-Sync enabled, frame time will\nnot go below 16ms");
    ImGui::Text("FPS: %i", FPS);
    ImGui::Separator();
    uiProcessNode(treeWhole);
    uiProcessNode(treeFixed);
    uiProcessNode(treeUpdate);
    uiProcessNode(treeDraw);
    uiProcessNode(treeSwap);
    ImGui::End();
}

void Profiler::logWindow() const {
    std::string log = Log::getContents();
    ImGui::Begin("Log", nullptr, ImVec2(0.34f*wsize.x, 0.31f*wsize.y), windowAlpha);
    ImGui::SetWindowPos(ImVec2(0.025f*wsize.x, 0.61f*wsize.y), ImGuiCond_FirstUseEver);
    ImGui::BeginChild("Log");
    ImGui::TextUnformatted(&(*log.begin()), &(*log.end()));
    ImGui::EndChild();
    ImGui::End();
}

void Profiler::uiProcessNode(const Profiler::Node& n) const {
    const Historial& nHist = hist.at(n.name);
    std::string currTime = toString(nHist.past[timeAvgOffset],4,2,true);
    std::string tag = std::string(n.name + " Time (curr: ") + currTime + " ms)";
    float max = 0.0f;
    for(int i = 0; i < PROFILER_HIST_SIZE; ++i) max = std::max(max, nHist.past[i]);
    if (ImGui::TreeNode((void*)nHist.id, "%s", tag.c_str())) {
        ImGui::PlotLines(std::string(toString(max, 1, 1, false) + "ms\n\n\n\n0 ms").c_str(), nHist.past, PROFILER_HIST_SIZE, timeAvgOffset, currTime.c_str(), 0.00f, max, vec2f(350,60));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("");
            ImGui::BeginTooltip();
            ImGui::Text("%s", n.desc.c_str());
            ImGui::EndTooltip();
        }
        for(const Node& child : n.children)
            uiProcessNode(child);
        ImGui::TreePop();
    }
}

Profiler::Watcher::Watcher() {
    //Update and draw first of all
    setUpdatePriority(-1000);
    setDrawPriority(-1000);
}

Profiler::Watcher::~Watcher() {
}

void Profiler::Watcher::fixedUpdate(float deltaTime) {
    (void) deltaTime;
    if(instance->currentNode != nullptr && instance->currentNode->name == "Swap") popMark(); //swap
    Profiler::instance->resetTreeFixed();
}

void Profiler::Watcher::update(float deltaTime) {
    (void) deltaTime;
    if(instance->currentNode != nullptr && instance->currentNode->name == "Swap") popMark(); //swap
    Profiler::instance->resetTreeUpdate();
}

void Profiler::Watcher::draw() const {
}
