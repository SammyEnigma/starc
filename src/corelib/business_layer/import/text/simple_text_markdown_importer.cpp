#include "simple_text_markdown_importer.h"

#include <business_layer/import/import_options.h>
#include <business_layer/model/text/text_model_xml.h>
#include <business_layer/templates/simple_text_template.h>
#include <domain/document_object.h>
#include <utils/helpers/text_helper.h>

#include <QFileInfo>
#include <QXmlStreamWriter>


namespace BusinessLayer {

AbstractSimpleTextImporter::SimpleText SimpleTextMarkdownImporter::importSimpleText(
    const ImportOptions& _options) const
{
    //
    // Открываем файл
    //
    QFile textFile(_options.filePath);
    if (!textFile.open(QIODevice::ReadOnly)) {
        return {};
    }

    //
    // Импортируем
    //
    SimpleText textDocument = importSimpleText(textFile.readAll());
    if (textDocument.name.isEmpty()) {
        textDocument.name = QFileInfo(_options.filePath).completeBaseName();
    }

    return textDocument;
}

AbstractSimpleTextImporter::SimpleText SimpleTextMarkdownImporter::importSimpleText(
    const QString& _text) const
{
    if (_text.simplified().isEmpty()) {
        return {};
    }

    SimpleText textDocument;

    //
    // Читаем plain text
    //
    // ... и пишем в документ
    //
    QXmlStreamWriter writer(&textDocument.text);
    writer.writeStartDocument();
    writer.writeStartElement(xml::kDocumentTag);
    writer.writeAttribute(xml::kMimeTypeAttribute,
                          Domain::mimeTypeFor(Domain::DocumentObjectType::SimpleText));
    writer.writeAttribute(xml::kVersionAttribute, "1.0");

    const QStringList paragraphs = QString(_text).remove('\r').split('\n');
    for (const auto& paragraph : paragraphs) {
        if (paragraph.simplified().isEmpty()) {
            continue;
        }

        writer.writeStartElement(toString(TextParagraphType::Text));
        writer.writeStartElement(xml::kValueTag);
        writer.writeCDATA(TextHelper::toHtmlEscaped(paragraph));
        writer.writeEndElement();
        writer.writeEndElement();
    }
    writer.writeEndDocument();

    return textDocument;
}

} // namespace BusinessLayer
