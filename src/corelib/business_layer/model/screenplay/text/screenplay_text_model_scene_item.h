#pragma once

#include "screenplay_text_model_item.h"

#include <QString>

class QDomElement;


namespace BusinessLayer
{

/**
 * @brief Класс элементов сцен модели сценария
 */
class CORE_LIBRARY_EXPORT ScreenplayTextModelSceneItem : public ScreenplayTextModelItem
{
public:
    /**
     * @brief Номер сцены
     */
    struct Number {
        QString value;
    };

public:
    ScreenplayTextModelSceneItem();
    explicit ScreenplayTextModelSceneItem(const QDomElement& _node);
    ~ScreenplayTextModelSceneItem() override;

    /**
     * @brief Номер сцены
     */
    void setNumber(int _number);
    Number number() const;

    /**
     * @brief Определяем интерфейс получения данных сцены
     */
    QVariant data(int _role) const override;

    /**
     * @brief Определяем интерфейс для получения XML блока
     */
    QString toXml() const override;

private:
    class Implementation;
    QScopedPointer<Implementation> d;
};

} // namespace BusinessLayer
