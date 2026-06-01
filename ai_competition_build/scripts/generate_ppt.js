const fs = require("fs");
const path = require("path");
const PptxGenJS = require("pptxgenjs");
const sharp = require("sharp");

const ROOT = path.resolve(__dirname, "..", "..");
const aiDir = fs.readdirSync(ROOT, { withFileTypes: true }).find((d) => d.isDirectory() && d.name.startsWith("ai")).name;
const AI = path.join(ROOT, aiDir);
const OUT = path.join(AI, "AI竞赛综合汇报_大图连续浏览SDK与展示平台.pptx");
const PREVIEW_DIR = path.join(AI, "AI竞赛综合汇报_PPT预览图");
fs.mkdirSync(PREVIEW_DIR, { recursive: true });

const logoPath = path.join(AI, "汉宁logo.png");
const sdkImgDir = findDir(AI, "大图连续浏览SDK参赛交付包", "软件界面图像");
const platformImgDir = findDir(AI, "展示平台参赛交付包", "软件界面图像");
const sdkImages = listImages(sdkImgDir);
const platformImages = listImages(platformImgDir);

const pptx = new PptxGenJS();
pptx.layout = "LAYOUT_WIDE";
pptx.author = "Codex";
pptx.subject = "AI竞赛综合汇报";
pptx.title = "AI赋能大图连续浏览SDK与地铁隧道展示平台";
pptx.company = "汉宁";
pptx.lang = "zh-CN";
pptx.theme = {
  headFontFace: "Microsoft YaHei",
  bodyFontFace: "Microsoft YaHei",
  lang: "zh-CN",
};
pptx.defineLayout({ name: "LAYOUT_WIDE", width: 13.333, height: 7.5 });

const C = {
  ink: "18212F",
  muted: "5F6B7A",
  soft: "EFF4F8",
  panel: "FFFFFF",
  line: "D7E1EA",
  blue: "1D5D9B",
  cyan: "2C9CDB",
  green: "22986B",
  yellow: "F2B84B",
  red: "C95555",
  navy: "0E2A3F",
  paleBlue: "E7F1FA",
  paleGreen: "E8F6F0",
};

function findDir(base, ...parts) {
  let current = base;
  for (const part of parts) {
    const next = fs.readdirSync(current, { withFileTypes: true })
      .find((d) => d.isDirectory() && d.name.includes(part));
    if (!next) throw new Error(`Missing directory segment: ${part}`);
    current = path.join(current, next.name);
  }
  return current;
}

function listImages(dir) {
  return fs.readdirSync(dir)
    .filter((name) => /\.(png|jpg|jpeg)$/i.test(name))
    .sort((a, b) => a.localeCompare(b, "zh-Hans-CN", { numeric: true }))
    .map((name) => path.join(dir, name));
}

function addBrand(slide, pageNo) {
  slide.addImage({ path: logoPath, x: 0.28, y: 0.18, w: 0.72, h: 0.36 });
  slide.addText(String(pageNo).padStart(2, "0"), {
    x: 12.15, y: 6.95, w: 0.55, h: 0.18, fontFace: "Aptos", fontSize: 8, color: C.muted, align: "right",
  });
  slide.addShape(pptx.ShapeType.line, {
    x: 0.35, y: 6.82, w: 12.55, h: 0,
    line: { color: C.line, width: 0.6 },
  });
}

function addTitle(slide, title, subtitle) {
  slide.addText(title, {
    x: 0.65, y: 0.72, w: 11.7, h: 0.48,
    fontFace: "Microsoft YaHei", fontSize: 24, bold: true, color: C.ink,
    fit: "shrink",
  });
  if (subtitle) {
    slide.addText(subtitle, {
      x: 0.68, y: 1.22, w: 11.6, h: 0.3,
      fontFace: "Microsoft YaHei", fontSize: 10.5, color: C.muted,
      fit: "shrink",
    });
  }
}

function addSectionLabel(slide, label, x, y, color = C.blue) {
  slide.addShape(pptx.ShapeType.roundRect, {
    x, y, w: 1.15, h: 0.28,
    rectRadius: 0.06,
    fill: { color },
    line: { color },
  });
  slide.addText(label, {
    x: x + 0.08, y: y + 0.06, w: 1.0, h: 0.12,
    fontFace: "Microsoft YaHei", fontSize: 7.6, bold: true, color: "FFFFFF", align: "center",
    margin: 0,
  });
}

function addBullets(slide, items, x, y, w, h, opts = {}) {
  const text = items.map((item) => `• ${item}`).join("\n");
  slide.addText(text, {
    x, y, w, h,
    fontFace: "Microsoft YaHei",
    fontSize: opts.size || 13,
    color: opts.color || C.ink,
    breakLine: false,
    fit: "shrink",
    valign: "mid",
    paraSpaceAfterPt: 8,
    margin: 0.04,
  });
}

function addMetric(slide, value, label, note, x, y, w, color) {
  slide.addText(value, {
    x, y, w, h: 0.44,
    fontFace: "Aptos Display", fontSize: 26, bold: true, color,
    fit: "shrink", margin: 0,
  });
  slide.addText(label, {
    x, y: y + 0.48, w, h: 0.2,
    fontFace: "Microsoft YaHei", fontSize: 8.5, bold: true, color: C.ink,
    fit: "shrink", margin: 0,
  });
  slide.addText(note, {
    x, y: y + 0.72, w, h: 0.32,
    fontFace: "Microsoft YaHei", fontSize: 7.4, color: C.muted,
    fit: "shrink", margin: 0,
  });
}

function addPanel(slide, x, y, w, h, fill = C.panel, line = C.line) {
  slide.addShape(pptx.ShapeType.roundRect, {
    x, y, w, h,
    rectRadius: 0.08,
    fill: { color: fill },
    line: { color: line, width: 0.8 },
  });
}

async function addImageContain(slide, imagePath, x, y, w, h, caption) {
  if (!imagePath || !fs.existsSync(imagePath)) {
    addPlaceholder(slide, x, y, w, h, caption || "待补充截图");
    return;
  }
  const meta = await sharp(imagePath).metadata();
  const ratio = meta.width / meta.height;
  let iw = w;
  let ih = w / ratio;
  if (ih > h) {
    ih = h;
    iw = h * ratio;
  }
  slide.addImage({ path: imagePath, x: x + (w - iw) / 2, y: y + (h - ih) / 2, w: iw, h: ih });
  slide.addShape(pptx.ShapeType.rect, {
    x, y, w, h,
    fill: { color: "FFFFFF", transparency: 100 },
    line: { color: C.line, transparency: 20, width: 0.8 },
  });
  if (caption) {
    slide.addText(caption, {
      x, y: y + h + 0.06, w, h: 0.22,
      fontFace: "Microsoft YaHei", fontSize: 7.6, color: C.muted,
      align: "center", fit: "shrink",
    });
  }
}

function addPlaceholder(slide, x, y, w, h, label) {
  addPanel(slide, x, y, w, h, "F7FAFC", "B8C6D3");
  slide.addText(label, {
    x: x + 0.2, y: y + h / 2 - 0.13, w: w - 0.4, h: 0.26,
    fontFace: "Microsoft YaHei", fontSize: 12, color: C.muted,
    align: "center", valign: "mid", fit: "shrink",
  });
}

function addMiniBar(slide, rows, x, y, w, maxValue, colorA, colorB) {
  rows.forEach((r, i) => {
    const top = y + i * 0.56;
    slide.addText(r.label, { x, y: top + 0.04, w: 2.2, h: 0.22, fontFace: "Microsoft YaHei", fontSize: 8.2, color: C.ink, fit: "shrink" });
    slide.addShape(pptx.ShapeType.rect, { x: x + 2.35, y: top + 0.08, w: (w - 3.1) * r.before / maxValue, h: 0.12, fill: { color: colorA }, line: { color: colorA } });
    slide.addShape(pptx.ShapeType.rect, { x: x + 2.35, y: top + 0.27, w: (w - 3.1) * r.after / maxValue, h: 0.12, fill: { color: colorB }, line: { color: colorB } });
    slide.addText(`${r.before}h / ${r.after}h`, { x: x + w - 0.65, y: top + 0.11, w: 0.62, h: 0.18, fontFace: "Aptos", fontSize: 7.4, color: C.muted, align: "right" });
  });
  slide.addText("传统估算", { x: x + 2.35, y: y + rows.length * 0.56 + 0.04, w: 0.75, h: 0.16, fontSize: 7.2, color: colorA, fontFace: "Microsoft YaHei" });
  slide.addText("AI协作", { x: x + 3.16, y: y + rows.length * 0.56 + 0.04, w: 0.75, h: 0.16, fontSize: 7.2, color: colorB, fontFace: "Microsoft YaHei" });
}

function addSimpleTable(slide, rows, x, y, w, colWidths, rowH = 0.42) {
  rows.forEach((row, r) => {
    let cx = x;
    row.forEach((cell, c) => {
      slide.addShape(pptx.ShapeType.rect, {
        x: cx, y: y + r * rowH, w: colWidths[c], h: rowH,
        fill: { color: r === 0 ? C.navy : (r % 2 ? "FFFFFF" : "F6F9FB") },
        line: { color: "DFE7EF", width: 0.4 },
      });
      slide.addText(String(cell), {
        x: cx + 0.07, y: y + r * rowH + 0.08, w: colWidths[c] - 0.14, h: rowH - 0.12,
        fontFace: "Microsoft YaHei",
        fontSize: r === 0 ? 7.7 : 7.4,
        bold: r === 0,
        color: r === 0 ? "FFFFFF" : C.ink,
        fit: "shrink",
        margin: 0,
      });
      cx += colWidths[c];
    });
  });
}

async function createSlides() {
  let page = 1;

  // 1 Cover
  {
    const slide = pptx.addSlide();
    slide.background = { color: "F3F7FA" };
    slide.addImage({ path: logoPath, x: 0.38, y: 0.28, w: 0.92, h: 0.46 });
    slide.addShape(pptx.ShapeType.rect, { x: 0, y: 0, w: 13.333, h: 7.5, fill: { color: "F3F7FA" }, line: { color: "F3F7FA" } });
    slide.addImage({ path: logoPath, x: 0.38, y: 0.28, w: 0.92, h: 0.46 });
    slide.addText("AI 赋能研发交付", {
      x: 0.75, y: 1.14, w: 7.2, h: 0.55,
      fontFace: "Microsoft YaHei", fontSize: 24, bold: true, color: C.blue,
    });
    slide.addText("大图连续浏览 SDK 与地铁隧道展示平台", {
      x: 0.72, y: 1.78, w: 8.7, h: 0.76,
      fontFace: "Microsoft YaHei", fontSize: 34, bold: true, color: C.ink, fit: "shrink",
    });
    slide.addText("按项目单元汇报 AI 工具、软件成果、量化指标与实践体会", {
      x: 0.75, y: 2.72, w: 7.9, h: 0.38,
      fontFace: "Microsoft YaHei", fontSize: 14, color: C.muted,
    });
    await addImageContain(slide, sdkImages[3], 7.65, 0.82, 4.95, 2.95, "");
    await addImageContain(slide, platformImages[4], 6.75, 4.02, 5.2, 2.25, "");
    slide.addShape(pptx.ShapeType.rect, { x: 0.78, y: 4.42, w: 4.8, h: 0.03, fill: { color: C.blue }, line: { color: C.blue } });
    slide.addText("参赛汇报材料  |  2026", { x: 0.78, y: 4.6, w: 4.6, h: 0.3, fontFace: "Microsoft YaHei", fontSize: 12, color: C.muted });
    slide.addText("资料口径：本地 README、参赛说明、既有展示平台交付包和截图素材", { x: 0.78, y: 6.55, w: 7.3, h: 0.22, fontFace: "Microsoft YaHei", fontSize: 8.5, color: C.muted });
    addBrand(slide, page++);
  }

  // 2 Integrated story
  {
    const slide = pptx.addSlide();
    slide.background = { color: "FFFFFF" };
    addBrand(slide, page++);
    addTitle(slide, "两项 AI 实践形成一条可复用链路", "大图浏览 SDK 解决底层影像浏览与标注，展示平台解决成果入库、接口和汇报展示。");
    const xs = [0.85, 3.75, 6.65, 9.55];
    const labels = [
      ["01 数据预处理", "ImageSlicer 将原始大图切片，形成缩略图、瓦片和元数据。"],
      ["02 SDK 浏览标注", "LOD + 动态加载 + 异步解码，实现 GB 级隧道影像浏览和病害标注。"],
      ["03 统一库与接口", "展示平台把台账、SQLite、图片和点云目录整理成可查询数据契约。"],
      ["04 可视化汇报", "Web 页面、Swagger 和截图材料支撑客户演示、验收与内部复盘。"],
    ];
    labels.forEach((item, i) => {
      addPanel(slide, xs[i], 2.05, 2.25, 2.55, i < 2 ? C.paleBlue : C.paleGreen, i < 2 ? "B8D7F0" : "BDE3D4");
      slide.addText(item[0], { x: xs[i] + 0.22, y: 2.35, w: 1.8, h: 0.26, fontFace: "Microsoft YaHei", fontSize: 12, bold: true, color: i < 2 ? C.blue : C.green, fit: "shrink" });
      slide.addText(item[1], { x: xs[i] + 0.22, y: 2.9, w: 1.82, h: 0.95, fontFace: "Microsoft YaHei", fontSize: 9.2, color: C.ink, fit: "shrink", breakLine: false });
      if (i < 3) {
        slide.addText("→", { x: xs[i] + 2.38, y: 2.96, w: 0.5, h: 0.35, fontFace: "Aptos", fontSize: 24, bold: true, color: "91A4B7" });
      }
    });
    addSectionLabel(slide, "汇报方式", 0.85, 5.16, C.navy);
    addBullets(slide, [
      "按项目单元展开：每个项目覆盖 AI 工具与沟通过程、软件介绍与运行效果、量化指标、结论体会。",
      "所有效率数据采用“本地证据 + 复盘估算”口径；可在赛前按真实工时记录微调。",
      "缺少的 AI 对话截图已保留占位，便于后续替换为真实 ChatGPT / Codex / Gemini 使用截图。",
    ], 0.85, 5.55, 11.65, 0.85, { size: 10.5 });
  }

  // 3 SDK AI process
  {
    const slide = pptx.addSlide();
    slide.background = { color: "FFFFFF" };
    addBrand(slide, page++);
    addTitle(slide, "项目一：大图连续浏览 SDK - AI 工具与沟通过程", "AI 参与需求拆解、架构选型、代码生成、性能策略和问题修复，人工负责业务判断和验收。");
    addSectionLabel(slide, "AI工具", 0.75, 1.78, C.blue);
    addBullets(slide, [
      "ChatGPT / Codex：用于架构讨论、Qt/C++ 代码实现、README 与竞赛材料整理。",
      "OpenCode 千问、MiniMax 等：用于局部 Bug 思路、替代方案和表达优化。",
      "人工把关：以真实编译、运行截图、业务规则和工程约束作为最终验收标准。",
    ], 0.75, 2.18, 5.0, 1.05, { size: 11 });
    addSectionLabel(slide, "沟通过程", 0.75, 3.65, C.green);
    addSimpleTable(slide, [
      ["阶段", "AI协作内容", "形成结果"],
      ["场景澄清", "说明 GB 级隧道影像浏览痛点", "明确切片、LOD、异步加载路线"],
      ["模块实现", "拆分 SDK、DemoApp、ImageSlicer", "形成可运行工程结构"],
      ["迭代修复", "围绕崩溃、编码、64位构建等问题排查", "README 与使用说明沉淀"],
    ], 0.75, 4.05, 5.6, [1.05, 2.25, 2.3], 0.43);
    addPlaceholder(slide, 6.85, 1.82, 5.55, 4.75, "待补充：AI 对话过程截图 / Codex 修改记录 / 关键提示词截图");
  }

  // 4 SDK software highlights
  {
    const slide = pptx.addSlide();
    slide.background = { color: "F8FBFD" };
    addBrand(slide, page++);
    addTitle(slide, "项目一：软件介绍与技术亮点", "面向 GB 级隧道全景图，提供切片预处理、连续浏览、缩放、标注和数据保存能力。");
    await addImageContain(slide, sdkImages[2], 0.75, 1.82, 5.45, 3.08, "缩略图 / LOD 浏览界面");
    await addImageContain(slide, sdkImages[3], 6.65, 1.82, 5.45, 3.08, "高清瓦片加载运行效果");
    addBullets(slide, [
      "Google Maps 式瓦片加载：只加载视野内切片，降低大图内存压力。",
      "LOD 机制：缩小时用缩略图快速定位，放大后异步加载高清切片。",
      "后台线程解码：图片读取与解码不阻塞主界面，保障浏览交互流畅。",
      "专业标注：支持折线、多边形、病害类型录入、右键管理和 JSON 保存。",
    ], 0.9, 5.38, 11.2, 0.92, { size: 10.4 });
  }

  // 5 SDK screenshots
  {
    const slide = pptx.addSlide();
    slide.background = { color: "FFFFFF" };
    addBrand(slide, page++);
    addTitle(slide, "项目一：运行效果截图", "从原始影像切片到 SDK 浏览，展示端到端可运行结果。");
    await addImageContain(slide, sdkImages[0], 0.75, 1.72, 3.65, 2.45, "导入数据库软件运行效果");
    await addImageContain(slide, sdkImages[1], 4.82, 1.72, 3.65, 2.45, "裁剪软件运行结果");
    await addImageContain(slide, sdkImages[2], 8.9, 1.72, 3.65, 2.45, "缩略图运行界面");
    await addImageContain(slide, sdkImages[3], 2.0, 4.72, 9.3, 1.55, "高清图像连续浏览截图");
  }

  // 6 SDK metrics
  {
    const slide = pptx.addSlide();
    slide.background = { color: "F7FAFC" };
    addBrand(slide, page++);
    addTitle(slide, "项目一：量化指标", "代码规模来自当前仓库统计；效率为复盘估算，建议赛前按真实记录校准。");
    addMetric(slide, "13,024", "源码/工程行数", "不含 .git、.vs、OpenCV 第三方头文件和参赛资料", 0.85, 1.85, 2.35, C.blue);
    addMetric(slide, "212", "相关文件数", "含 SDK、DemoApp、ImageSlicer 工程与源码文件", 3.55, 1.85, 2.25, C.green);
    addMetric(slide, "85h", "节省工时估算", "传统 120h / AI 协作 35h", 6.15, 1.85, 2.25, C.yellow);
    addMetric(slide, "71%", "效率提升估算", "以节省工时 / 传统工时计算", 8.78, 1.85, 2.25, C.red);
    addMetric(slide, "4", "运行截图证据", "切片、导入、缩略图、高清浏览", 11.1, 1.85, 1.45, C.cyan);
    addPanel(slide, 0.85, 3.42, 5.85, 2.65, "FFFFFF", "DCE6EF");
    addSectionLabel(slide, "模块代码分布", 1.05, 3.68, C.blue);
    addMiniBar(slide, [
      { label: "SDK 主体", before: 72, after: 24 },
      { label: "DemoApp", before: 34, after: 7 },
      { label: "ImageSlicer", before: 22, after: 4 },
    ], 1.05, 4.08, 5.3, 72, "A7B5C5", C.blue);
    slide.addText("注：左侧小时为按模块复杂度折算的传统/AI协作估算，用于答辩表达 AI 价值，不替代真实工时记录。", {
      x: 1.05, y: 5.82, w: 5.2, h: 0.22, fontFace: "Microsoft YaHei", fontSize: 7.2, color: C.muted,
    });
    addPanel(slide, 7.05, 3.42, 5.5, 2.65, "FFFFFF", "DCE6EF");
    addSectionLabel(slide, "可量化能力", 7.25, 3.68, C.green);
    addBullets(slide, [
      "支持 GB 级超高分辨率隧道影像的切片化浏览。",
      "LOD + 动态切片加载降低内存占用和等待时间。",
      "README 中明确要求 64-bit 构建，避免大图处理 2GB 内存限制。",
      "病害定位采用二分查找思路，面向图幅归属做快速定位。",
    ], 7.25, 4.1, 4.95, 1.35, { size: 9.2 });
  }

  // 7 SDK conclusion
  {
    const slide = pptx.addSlide();
    slide.background = { color: "FFFFFF" };
    addBrand(slide, page++);
    addTitle(slide, "项目一：结论及体会", "AI 的价值不只在生成代码，而在把性能策略、工程结构、调试经验和说明文档串成闭环。");
    addPanel(slide, 0.85, 1.78, 3.6, 3.55, C.paleBlue, "BCD7EC");
    addPanel(slide, 4.88, 1.78, 3.6, 3.55, C.paleGreen, "B9E2D1");
    addPanel(slide, 8.9, 1.78, 3.6, 3.55, "FFF7E6", "F0D79B");
    slide.addText("结论", { x: 1.12, y: 2.14, w: 3.0, h: 0.28, fontSize: 16, bold: true, color: C.blue, fontFace: "Microsoft YaHei" });
    slide.addText("已形成可演示的大图切片、缩略图浏览、高清瓦片加载和病害标注能力，可作为后续展示平台或检测业务的底层图像能力。", { x: 1.12, y: 2.72, w: 3.0, h: 1.35, fontSize: 12, color: C.ink, fontFace: "Microsoft YaHei", fit: "shrink" });
    slide.addText("体会", { x: 5.15, y: 2.14, w: 3.0, h: 0.28, fontSize: 16, bold: true, color: C.green, fontFace: "Microsoft YaHei" });
    slide.addText("对 AI 说明清楚数据规模、构建环境和交互目标后，AI 更适合承担架构拆分、重复代码、问题定位和文档沉淀。", { x: 5.15, y: 2.72, w: 3.0, h: 1.35, fontSize: 12, color: C.ink, fontFace: "Microsoft YaHei", fit: "shrink" });
    slide.addText("建议", { x: 9.17, y: 2.14, w: 3.0, h: 0.28, fontSize: 16, bold: true, color: "A46A00", fontFace: "Microsoft YaHei" });
    slide.addText("保留真实提示词、构建错误、运行截图和工时记录。AI 生成内容必须经过编译、真实数据和人工业务规则三重校验。", { x: 9.17, y: 2.72, w: 3.0, h: 1.35, fontSize: 12, color: C.ink, fontFace: "Microsoft YaHei", fit: "shrink" });
  }

  // 8 platform AI process
  {
    const slide = pptx.addSlide();
    slide.background = { color: "FFFFFF" };
    addBrand(slide, page++);
    addTitle(slide, "项目二：展示平台 - AI 工具与沟通过程", "从编译失败、数据映射不一致到可演示平台，AI 参与诊断、实现、验证和材料沉淀。");
    await addImageContain(slide, platformImages[0], 0.75, 1.75, 4.0, 2.85, "项目代码列表 / 工程规模证据");
    addBullets(slide, [
      "Codex：阅读代码、定位编译错误、修改实体/DTO/Controller/Service、补齐文档。",
      "ChatGPT / Gemini：用于界面构思、架构讨论、表达优化和风险治理补充。",
      "沟通输入包括真实报错、业务规则、样例目录、概要设计和人工验收反馈。",
    ], 5.15, 1.82, 6.8, 1.18, { size: 11 });
    addSimpleTable(slide, [
      ["协作阶段", "问题", "AI输出"],
      ["诊断", "新旧实体、数据库表和前端展示不一致", "影响面分析和修改清单"],
      ["实现", "台账同步、SQLite解析、统一库、接口和页面联动", "跨模块代码补齐"],
      ["验证", "构建错误、样例导入、Swagger、页面截图", "循环修复与材料沉淀"],
    ], 5.15, 3.38, 6.65, [1.28, 2.65, 2.72], 0.46);
    addPlaceholder(slide, 0.75, 5.12, 11.3, 1.15, "待补充：Codex / ChatGPT / Gemini 对话过程截图，可放 2-3 张关键沟通截图");
  }

  // 9 platform introduction
  {
    const slide = pptx.addSlide();
    slide.background = { color: "F8FBFD" };
    addBrand(slide, page++);
    addTitle(slide, "项目二：软件介绍与技术亮点", "展示平台面向成果汇报和客户演示，将台账、SQLite、图片和点云目录转成统一查询与可视化能力。");
    await addImageContain(slide, platformImages[3], 0.75, 1.72, 5.6, 2.85, "工程信息界面");
    await addImageContain(slide, platformImages[4], 6.75, 1.72, 5.6, 2.85, "隧道区间浏览界面");
    addBullets(slide, [
      "链路闭环：WinForms 上传工具、统一库、WebAPI、Web 前端和 Swagger 接口。",
      "数据契约：以工程实例、站点/区间、病害、环片、灰度图和高清图组织成果。",
      "演示友好：工程地图、区间浏览、病害列表、高清图弹窗和接口文档可直接展示。",
      "风险治理：AI 生成内容以概要设计、真实表结构、构建和样例导入验证为准。",
    ], 0.9, 5.03, 11.25, 1.02, { size: 10.2 });
  }

  // 10 platform screenshots
  {
    const slide = pptx.addSlide();
    slide.background = { color: "FFFFFF" };
    addBrand(slide, page++);
    addTitle(slide, "项目二：运行效果截图", "多端能力覆盖上传工具、Web 登录、工程信息、区间浏览和 Swagger 接口。");
    await addImageContain(slide, platformImages[1], 0.72, 1.68, 3.0, 1.72, "上传工具");
    await addImageContain(slide, platformImages[2], 4.0, 1.68, 3.0, 1.72, "登录界面");
    await addImageContain(slide, platformImages[3], 7.28, 1.68, 3.0, 1.72, "工程信息");
    await addImageContain(slide, platformImages[4], 1.1, 4.15, 5.15, 1.95, "隧道区间浏览");
    await addImageContain(slide, platformImages[5], 7.1, 4.15, 4.4, 1.95, "Swagger 接口");
  }

  // 11 platform metrics
  {
    const slide = pptx.addSlide();
    slide.background = { color: "F7FAFC" };
    addBrand(slide, page++);
    addTitle(slide, "项目二：量化指标", "指标来自既有《02_效果数据_量化指标.xlsx》，效率口径为复盘估算。");
    addMetric(slide, "76%", "效率提升", "传统估算 162h / AI 协作 39h", 0.85, 1.78, 2.05, C.blue);
    addMetric(slide, "123h", "节省工时", "用于答辩表达，可赛前校准", 3.12, 1.78, 2.05, C.green);
    addMetric(slide, "23", "API接口", "Controller 中公开 HTTP 接口", 5.4, 1.78, 1.65, C.cyan);
    addMetric(slide, "1,461", "病害记录", "进入统一查询链路", 7.36, 1.78, 1.85, C.red);
    addMetric(slide, "1.8GB", "样例数据", "763 个样例文件", 9.48, 1.78, 1.85, C.yellow);
    addMetric(slide, "12,215", "源码行数", "不含日志和运行输出", 11.25, 1.78, 1.45, C.blue);
    addPanel(slide, 0.85, 3.35, 6.0, 2.72, "FFFFFF", "DCE6EF");
    addSectionLabel(slide, "效率测算", 1.05, 3.62, C.blue);
    addMiniBar(slide, [
      { label: "需求拆解", before: 8, after: 1.5 },
      { label: "数据库映射", before: 24, after: 5 },
      { label: "上传工具", before: 20, after: 4.5 },
      { label: "SQLite入库", before: 40, after: 11 },
      { label: "前端展示", before: 36, after: 9 },
    ], 1.05, 4.02, 5.35, 40, "A7B5C5", C.blue);
    addPanel(slide, 7.25, 3.35, 5.25, 2.72, "FFFFFF", "DCE6EF");
    addSectionLabel(slide, "项目数据", 7.48, 3.62, C.green);
    addSimpleTable(slide, [
      ["指标", "数值"],
      ["项目实例", "2"],
      ["站点/区间实体", "7"],
      ["已上传实体", "3"],
      ["环片记录", "1,258"],
      ["灰度图索引", "34"],
    ], 7.48, 4.02, 4.68, [2.6, 2.08], 0.32);
  }

  // 12 platform conclusion
  {
    const slide = pptx.addSlide();
    slide.background = { color: "FFFFFF" };
    addBrand(slide, page++);
    addTitle(slide, "项目二：结论及体会", "AI 加速了从业务资料到可运行系统的闭环，但核心规则仍需人工验收。");
    addBullets(slide, [
      "结论：展示平台已形成上传、入库、接口、页面和材料沉淀的演示闭环。",
      "体会：AI 最适合承担跨模块定位、样板代码生成、文档整理和反复修复。",
      "边界：涉及 SQLite 字段含义、Word 表语义、工程数据安全时，必须以真实数据和人工规则为准。",
      "推广：适合复制到“多源文件 + 结构化入库 + 可视化展示 + 汇报沉淀”的业务场景。",
    ], 0.9, 1.85, 6.0, 2.0, { size: 13 });
    addSimpleTable(slide, [
      ["AI风险", "控制措施", "竞赛表达"],
      ["幻觉/误映射", "以概要设计和真实表结构验证", "AI加速，人工把关"],
      ["数据安全", "本地资料、本地运行", "适合内网部署"],
      ["质量一致性", "构建、接口、页面检查", "形成可复跑清单"],
      ["知识沉淀", "报告、日志、API说明", "个人试错转为团队资产"],
    ], 7.15, 1.9, 5.25, [1.18, 2.05, 2.02], 0.47);
  }

  // 13 overall value
  {
    const slide = pptx.addSlide();
    slide.background = { color: "F3F7FA" };
    addBrand(slide, page++);
    addTitle(slide, "综合结论：AI 已经进入研发交付主流程", "两项项目证明，AI 能把需求拆解、代码生成、调试验证和竞赛材料沉淀联成一套方法。");
    addPanel(slide, 0.85, 1.85, 3.35, 3.55, "FFFFFF", "DCE6EF");
    addPanel(slide, 4.95, 1.85, 3.35, 3.55, "FFFFFF", "DCE6EF");
    addPanel(slide, 9.05, 1.85, 3.35, 3.55, "FFFFFF", "DCE6EF");
    slide.addText("可复制方法", { x: 1.15, y: 2.18, w: 2.6, h: 0.3, fontSize: 16, bold: true, color: C.blue, fontFace: "Microsoft YaHei" });
    slide.addText("用真实业务资料驱动 AI，不只问“怎么写代码”，而是持续提供报错、截图、数据结构和验收结果。", { x: 1.15, y: 2.82, w: 2.68, h: 1.25, fontSize: 12, color: C.ink, fontFace: "Microsoft YaHei", fit: "shrink" });
    slide.addText("可量化价值", { x: 5.25, y: 2.18, w: 2.6, h: 0.3, fontSize: 16, bold: true, color: C.green, fontFace: "Microsoft YaHei" });
    slide.addText("SDK：约 71% 效率提升估算；展示平台：约 76% 效率提升估算。两者均需保留真实工时记录持续校准。", { x: 5.25, y: 2.82, w: 2.68, h: 1.25, fontSize: 12, color: C.ink, fontFace: "Microsoft YaHei", fit: "shrink" });
    slide.addText("后续建议", { x: 9.35, y: 2.18, w: 2.6, h: 0.3, fontSize: 16, bold: true, color: "A46A00", fontFace: "Microsoft YaHei" });
    slide.addText("补齐 AI 对话截图、真实工时台账、演示录屏和脱敏检查，把本次竞赛材料升级为部门 AI 研发实践模板。", { x: 9.35, y: 2.82, w: 2.68, h: 1.25, fontSize: 12, color: C.ink, fontFace: "Microsoft YaHei", fit: "shrink" });
    slide.addText("建议答辩主线：痛点 → AI 协作方法 → 两个项目成果 → 数据指标 → 风险治理 → 推广价值", {
      x: 0.92, y: 6.0, w: 11.5, h: 0.36, fontFace: "Microsoft YaHei", fontSize: 13, bold: true, color: C.navy, align: "center",
    });
  }
}

async function writePreviews() {
  const slides = [
    ["01", "AI 赋能研发交付", "大图连续浏览 SDK 与地铁隧道展示平台"],
    ["02", "两项 AI 实践形成一条可复用链路", "切片 / 浏览标注 / 统一库接口 / 可视化汇报"],
    ["03", "项目一：AI 工具与沟通过程", "保留 AI 对话截图占位"],
    ["04", "项目一：软件介绍与技术亮点", "LOD、动态瓦片、异步加载、专业标注"],
    ["05", "项目一：运行效果截图", "切片、缩略图、高清浏览"],
    ["06", "项目一：量化指标", "13,024 行，约 71% 效率提升估算"],
    ["07", "项目一：结论及体会", "AI 串联性能策略、工程结构和文档"],
    ["08", "项目二：AI 工具与沟通过程", "从编译失败到可演示系统"],
    ["09", "项目二：软件介绍与技术亮点", "统一库、接口、Web 展示"],
    ["10", "项目二：运行效果截图", "上传工具、页面、Swagger"],
    ["11", "项目二：量化指标", "76% 效率提升，节省 123h"],
    ["12", "项目二：结论及体会", "AI 加速，人工验收把关"],
    ["13", "综合结论", "AI 已进入研发交付主流程"],
  ];
  for (const [no, title, sub] of slides) {
    const svg = `<svg width="1600" height="900" xmlns="http://www.w3.org/2000/svg">
      <rect width="1600" height="900" fill="#F7FAFC"/>
      <rect x="74" y="76" width="1452" height="748" rx="22" fill="#FFFFFF" stroke="#D7E1EA"/>
      <text x="108" y="146" fill="#1D5D9B" font-size="36" font-family="Microsoft YaHei, Arial" font-weight="700">${escapeXml(no)}</text>
      <text x="108" y="280" fill="#18212F" font-size="54" font-family="Microsoft YaHei, Arial" font-weight="700">${escapeXml(title)}</text>
      <text x="110" y="355" fill="#5F6B7A" font-size="28" font-family="Microsoft YaHei, Arial">${escapeXml(sub)}</text>
      <rect x="110" y="620" width="980" height="14" fill="#1D5D9B"/>
      <text x="110" y="705" fill="#5F6B7A" font-size="24" font-family="Microsoft YaHei, Arial">AI竞赛综合汇报 PPT 预览图，用于快速核对页序和主题。</text>
    </svg>`;
    await sharp(Buffer.from(svg)).png().toFile(path.join(PREVIEW_DIR, `slide-${no}.png`));
  }
}

function escapeXml(value) {
  return String(value)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}

(async () => {
  await createSlides();
  await pptx.writeFile({ fileName: OUT });
  await writePreviews();
  console.log(JSON.stringify({ pptx: OUT, previewDir: PREVIEW_DIR, slides: pptx._slides.length }, null, 2));
})();
