#ifndef RHITEXTUREITEM_H
#define RHITEXTUREITEM_H

#include <QQuickRhiItem>
#include <rhi/qrhi.h>

class ExampleRhiItemRenderer : public QQuickRhiItemRenderer
{
public:
    void initialize(QRhiCommandBuffer *cb) override;
    void synchronize(QQuickRhiItem *item) override;
    void render(QRhiCommandBuffer *cb) override;

private:
    QRhi *m_rhi = nullptr;
    int m_sampleCount = 1;
    QRhiTexture::Format m_textureFormat = QRhiTexture::RGBA8;

    std::unique_ptr<QRhiBuffer> m_vbuf;
    std::unique_ptr<QRhiBuffer> m_ubuf;
    std::unique_ptr<QRhiSampler> m_sampler;
    std::unique_ptr<QRhiTexture> m_tex;
    std::unique_ptr<QRhiShaderResourceBindings> m_srb;
    std::unique_ptr<QRhiGraphicsPipeline> m_pipeline;

    QMatrix4x4 m_viewProjection;
    float m_angle = 0.0f;
    float m_alpha = 1.0f;

    QByteArray m_pendingPixels;
    QSize m_pendingSize;
    bool m_hasPending = false;
};

class ExampleRhiItem : public QQuickRhiItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(RhiTextureItem)
    Q_PROPERTY(float angle READ angle WRITE setAngle NOTIFY angleChanged)
    Q_PROPERTY(float backgroundAlpha READ backgroundAlpha WRITE setBackgroundAlpha NOTIFY backgroundAlphaChanged)

public:
    QQuickRhiItemRenderer *createRenderer() override;
    ExampleRhiItem() {
        setImplicitWidth(640);
        setImplicitHeight(360);
    }

    Q_INVOKABLE void setFrameRGBA8(const QByteArray &pixels, int w, int h);
    bool takePendingFrame(QByteArray &out, QSize &outSize);
    float angle() const { return m_angle; }
    void setAngle(float a);

    float backgroundAlpha() const { return m_alpha; }
    void setBackgroundAlpha(float a);

signals:
    void angleChanged();
    void backgroundAlphaChanged();

private:
    float m_angle = 0.0f;
    float m_alpha = 1.0f;
};

#endif
