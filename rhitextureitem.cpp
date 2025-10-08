#include "rhitextureitem.h"
#include <QFile>

//![0]
QQuickRhiItemRenderer *ExampleRhiItem::createRenderer() {
    return new ExampleRhiItemRenderer;
}
void ExampleRhiItem::setFrameRGBA8(const QByteArray &pixels, int w, int h) {
    // Called on GUI thread , maybeuse QMetaObject::invokeMethod with QueuedConnection
    setProperty("_px", pixels);
    setProperty("_sz", QSize(w, h));
    update();
}

bool ExampleRhiItem::takePendingFrame(QByteArray &out, QSize &outSize)
{
    auto px = property("_px");
    auto sz = property("_sz");
    if (!px.isValid() || !sz.isValid()) return false;
    out = px.toByteArray();
    outSize = sz.toSize();
    setProperty("_px", QVariant());
    setProperty("_sz", QVariant());
    return !out.isEmpty() && outSize.isValid();
}

void ExampleRhiItem::setAngle(float a) {
    if (m_angle == a)
        return;

    m_angle = a;
    emit angleChanged();
    update();
}

void ExampleRhiItem::setBackgroundAlpha(float a) {
    if (m_alpha == a)
        return;

    m_alpha = a;
    emit backgroundAlphaChanged();
    update();
}

void ExampleRhiItemRenderer::synchronize(QQuickRhiItem *rhiItem) {
    // may need more thread shit here tbh
    //From a non-GUI thread: convert your cv::Mat to RGBA8, wrap in QByteArray, then
    //QMetaObject::invokeMethod(rhiItem, "setFrameRGBA8", Qt::QueuedConnection, Q_ARG(QByteArray, payload), Q_ARG(int, w), Q_ARG(int, h));

    auto *item = static_cast<ExampleRhiItem *>(rhiItem);
    if (item->angle() != m_angle) m_angle = item->angle();
    if (item->backgroundAlpha() != m_alpha) m_alpha = item->backgroundAlpha();

    QByteArray px;
    QSize sz;
    if (item->takePendingFrame(px, sz)) {
        m_pendingPixels = std::move(px);
        m_pendingSize = sz;
        m_hasPending = true;
    }


}
//![0]

static QShader getShader(const QString &name) {
    QFile f(name);
    return f.open(QIODevice::ReadOnly) ? QShader::fromSerialized(f.readAll()) : QShader();
}

static float vertexData[] = {
    //   x,     y,    u,   v
     0.0f,   0.5f,  0.5f, 0.0f,
    -0.5f,  -0.5f,  0.0f, 1.0f,
     0.5f,  -0.5f,  1.0f, 1.0f
};

//![1]
void ExampleRhiItemRenderer::initialize(QRhiCommandBuffer *cb) {
    if (m_rhi != rhi()) {
        m_rhi = rhi();
        m_pipeline.reset();
    }

    if (m_sampleCount != renderTarget()->sampleCount()) {
        m_sampleCount = renderTarget()->sampleCount();
        m_pipeline.reset();
    }

    QRhiTexture *finalTex = m_sampleCount > 1 ? resolveTexture() : colorTexture();
    if (m_textureFormat != finalTex->format()) {
        m_textureFormat = finalTex->format();
        m_pipeline.reset();
    }
//![1]
//![2]
    if (!m_pipeline) {
        m_vbuf.reset(m_rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(vertexData)));
        m_vbuf->create();

        m_ubuf.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 64));
        m_ubuf->create();


        m_sampler.reset(m_rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
        m_sampler->create();

        QSize texSize = {1, 1};
        m_tex.reset(m_rhi->newTexture(QRhiTexture::RGBA8, texSize, 1));
        m_tex->create();

        {
            QRhiResourceUpdateBatch *u = m_rhi->nextResourceUpdateBatch();
            const quint32 px = 0xFFFFF000u;
            QRhiTextureSubresourceUploadDescription sub(
                QByteArray(reinterpret_cast<const char*>(&px), 4)
                );
            QRhiTextureUploadDescription desc(QRhiTextureUploadEntry(0, 0, sub));
            u->uploadTexture(m_tex.get(), desc);
            cb->resourceUpdate(u);
        }
        if(false){
            auto *u = m_rhi->nextResourceUpdateBatch();
            const quint32 px = 0xFF000000u; // RGBA8 black
            QRhiTextureSubresourceUploadDescription sub(QByteArray(reinterpret_cast<const char*>(&px), 4));
            QRhiTextureUploadDescription desc(QRhiTextureUploadEntry(0, 0, sub));
            u->uploadTexture(m_tex.get(), desc);
            cb->resourceUpdate(u);
        }

        m_srb.reset(m_rhi->newShaderResourceBindings());
        m_srb->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, m_ubuf.get()),
            QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_tex.get(), m_sampler.get()),
        });
        m_srb->create();

        m_pipeline.reset(m_rhi->newGraphicsPipeline());
        const QShader vs = getShader(":/scenegraph/rhitextureitem/shaders/frame.vert.qsb");
        const QShader fs = getShader(":/scenegraph/rhitextureitem/shaders/frame.frag.qsb");
        if (!vs.isValid() || !fs.isValid()) {
            qFatal("QSB shader(s) missing or invalid. Check resource path & qsb output.");
        }
        Q_ASSERT(vs.isValid());
        Q_ASSERT(fs.isValid());
        m_pipeline->setShaderStages({
           { QRhiShaderStage::Vertex, vs },
           { QRhiShaderStage::Fragment, fs }
        });
        QRhiVertexInputLayout inputLayout;
        inputLayout.setBindings({
            { 4 * sizeof(float) }
        });
        inputLayout.setAttributes({
            { 0, 0, QRhiVertexInputAttribute::Float2, 0 },
            { 0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float) }
        });
        m_pipeline->setSampleCount(m_sampleCount);
        m_pipeline->setVertexInputLayout(inputLayout);
        m_pipeline->setShaderResourceBindings(m_srb.get());
        m_pipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
        m_pipeline->create();

        QRhiResourceUpdateBatch *resourceUpdates = m_rhi->nextResourceUpdateBatch();
        resourceUpdates->uploadStaticBuffer(m_vbuf.get(), vertexData);
        cb->resourceUpdate(resourceUpdates);
    }

    const QSize outputSize = renderTarget()->pixelSize();
    m_viewProjection = m_rhi->clipSpaceCorrMatrix();
    m_viewProjection.perspective(45.0f, outputSize.width() / (float) outputSize.height(), 0.01f, 1000.0f);
    m_viewProjection.translate(0, 0, -2);
//![2]
}

//![3]
void ExampleRhiItemRenderer::render(QRhiCommandBuffer *cb) {
    if (m_hasPending) {
        if (!m_tex || m_tex->pixelSize() != m_pendingSize) {
            m_tex.reset();
            m_tex.reset(m_rhi->newTexture(QRhiTexture::RGBA8, m_pendingSize, 1));
            m_tex->create();

            // Rebuild 8SRB with the real texture
            m_srb.reset(m_rhi->newShaderResourceBindings());
            m_srb->setBindings({
                QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, m_ubuf.get()),
                QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_tex.get(), m_sampler.get())
            });
            m_srb->create();
            m_pipeline->setShaderResourceBindings(m_srb.get());
        }

        QRhiResourceUpdateBatch *u = m_rhi->nextResourceUpdateBatch();

        QRhiTextureSubresourceUploadDescription sub(m_pendingPixels);
        // tightly packed?
        sub.setDataStride(static_cast<quint32>(m_pendingSize.width() * 4));  // RGBA8

        QRhiTextureUploadEntry entry(0, 0, sub);
        QRhiTextureUploadDescription desc(entry);
        u->uploadTexture(m_tex.get(), desc);
        cb->resourceUpdate(u);

        m_hasPending = false;
        m_pendingPixels.clear();
    }

    QRhiResourceUpdateBatch *resourceUpdates = m_rhi->nextResourceUpdateBatch();

    // update uniforms pleaseee
    QMatrix4x4 modelViewProjection = m_viewProjection;
    modelViewProjection.rotate(m_angle, 0, 1, 0);
    resourceUpdates->updateDynamicBuffer(m_ubuf.get(), 0, 64, modelViewProjection.constData());

    // Qt Quick expects premultiplied alpha
    const QColor clearColor = QColor::fromRgbF(0.5f * m_alpha, 0.5f * m_alpha, 0.7f * m_alpha, m_alpha);
    cb->beginPass(renderTarget(), clearColor, { 1.0f, 0 }, resourceUpdates);

    cb->setGraphicsPipeline(m_pipeline.get());
    const QSize outputSize = renderTarget()->pixelSize();
    cb->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));
    cb->setShaderResources(m_srb.get());
    const QRhiCommandBuffer::VertexInput vbufBinding(m_vbuf.get(), 0);
    cb->setVertexInput(0, 1, &vbufBinding);
    cb->draw(3);

    cb->endPass();
}
//![3]
