/**
 * @file KeyMapSidePanel.h
 * @brief 侧边键位面板 — 替代 ToolForm 的键位编辑模式
 *
 * 从 VideoForm 右侧滑入，包含:
 * - 配置选择/CRUD
 * - 可拖拽键位组件
 * - 拟人参数滑块
 * - 显示设置
 */
#ifndef KEYMAPSIDEPANEL_H
#define KEYMAPSIDEPANEL_H

#include <QWidget>
#include <QPropertyAnimation>
#include "FluentComboBox.h"
#include <QPushButton>
#include <QLabel>

// 前向声明
namespace Fluent {
class FluentButton;
class FluentSlider;
class FluentToggle;
class FluentCard;
}

// 与 ToolForm 中的 DraggableLabel 兼容
class DraggableLabel;

class KeyMapSidePanel : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int panelWidth READ panelWidth WRITE setPanelWidth)
public:
    explicit KeyMapSidePanel(QWidget* parent = nullptr);

    void setExpanded(bool expanded);
    bool isExpanded() const { return m_expanded; }

    int panelWidth() const { return m_currentWidth; }
    void setPanelWidth(int w);

    // 设计展开宽度（消除 videoform 中的硬编码 260）
    int expandedWidth() const { return m_expandedWidth; }
    // 自适应可用宽度，面板宽度被夹取在 [minWidth, min(expandedWidth, available)] 并受 maxWidth 上限约束
    int minWidth() const { return m_minWidth; }
    int maxWidth() const { return m_maxWidth; }
    void setPanelWidthAdaptive(int available);

    void refreshConfigList();
    QString currentConfig() const;
    void setCurrentConfig(const QString& filename);
    void setOverlayButtonState(bool checked);

    // 外部同步设置 (从 VideoSettingsPopup)
    void setOverlayChecked(bool checked);
    void setOverlayOpacity(int value);
    void setTipOpacity(int value);

signals:
    void configChanged(const QString& filename);
    void saveRequested();
    void overlayToggled(bool visible);
    void overlayOpacityChanged(int value);
    void scriptTipOpacityChanged(int value);
    void editModeChanged(bool active);
    void closeRequested();

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private:
    void setupUI();

    bool m_expanded = false;
    int m_currentWidth = 0;
    int m_expandedWidth = 260;
    int m_minWidth = 220;     // 低于此值控件挤压，改由内部 QScrollArea 滚动，不裁切
    int m_maxWidth = 320;     // 上限，避免过宽遮挡视频区

    QPropertyAnimation* m_expandAnim = nullptr;

    // 配置管理
    Fluent::FluentComboBox* m_configCombo = nullptr;
    QPushButton* m_newBtn = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    QPushButton* m_folderBtn = nullptr;
    Fluent::FluentButton* m_saveBtn = nullptr;

    // 显示键位
    Fluent::FluentToggle* m_overlayToggle = nullptr;

    // 拟人参数
    Fluent::FluentSlider* m_randomOffsetSlider = nullptr;
    Fluent::FluentSlider* m_steerSmoothSlider = nullptr;
    Fluent::FluentSlider* m_steerCurveSlider = nullptr;
    Fluent::FluentSlider* m_steerHumanizeSlider = nullptr;
    Fluent::FluentSlider* m_slideCurveSlider = nullptr;

    // 辅助压枪（引擎原生实现，全局参数）
    Fluent::FluentToggle* m_recoilEnabledToggle = nullptr;
    Fluent::FluentSlider* m_recoilVerticalSlider = nullptr;
    Fluent::FluentSlider* m_recoilHorizontalSlider = nullptr;
    Fluent::FluentSlider* m_recoilStrengthSlider = nullptr;
    Fluent::FluentSlider* m_recoilDragXSlider = nullptr;
    Fluent::FluentSlider* m_recoilDragYSlider = nullptr;
    Fluent::FluentSlider* m_recoilStepIntervalSlider = nullptr;
    Fluent::FluentSlider* m_recoilRandomizeStepSlider = nullptr;
    Fluent::FluentSlider* m_recoilJitterSlider = nullptr;
    Fluent::FluentSlider* m_recoilCurveSlider = nullptr;
    Fluent::FluentSlider* m_recoilMaxDistanceSlider = nullptr;
    Fluent::FluentSlider* m_recoilRampUpSlider = nullptr;
    Fluent::FluentSlider* m_recoilRampUpStartSlider = nullptr;
    Fluent::FluentSlider* m_recoilDecaySlider = nullptr;
    Fluent::FluentSlider* m_recoilDecayEndSlider = nullptr;
    Fluent::FluentSlider* m_recoilReleaseGraceSlider = nullptr;

    // 透明度
    Fluent::FluentSlider* m_overlayOpacitySlider = nullptr;
    Fluent::FluentSlider* m_tipOpacitySlider = nullptr;

    // 可拖拽组件
    DraggableLabel* m_clickLabel = nullptr;
    DraggableLabel* m_holdLabel = nullptr;
    DraggableLabel* m_scriptLabel = nullptr;
    DraggableLabel* m_steerLabel = nullptr;
    DraggableLabel* m_cameraLabel = nullptr;
    DraggableLabel* m_freeLookLabel = nullptr;
    DraggableLabel* m_recoilLabel = nullptr;
};

#endif
