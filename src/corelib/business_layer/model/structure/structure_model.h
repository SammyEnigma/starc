#pragma once

#include "../abstract_model.h"


namespace Domain {
enum class DocumentObjectType;
}

namespace BusinessLayer
{

class StructureModelItem;

/**
 * @brief Модель структуры документов проекта
 */
class StructureModel : public AbstractModel
{
    Q_OBJECT

public:
    explicit StructureModel(QObject* _parent = nullptr);
    ~StructureModel() override;

    /**
     * @brief Добавить документ
     */
    void addDocument(Domain::DocumentObjectType _type, const QString& _name, const QModelIndex& _parent = {});

    /**
     * @brief Добавить элемент в начало
     */
    void prependItem(StructureModelItem* _item, StructureModelItem* _parentItem = nullptr);

    /**
     * @brief Добавить элемент в конец
     */
    void appendItem(StructureModelItem* _item, StructureModelItem* _parentItem = nullptr);

    /**
     * @brief Вставить элемент после родственика
     */
    void insertItem(StructureModelItem* _item, StructureModelItem* _afterSiblingItem);

    /**
     * @brief Удалить элемент
     */
    void removeItem(StructureModelItem* _item);

    /**
     * @brief Обновить элемент
     */
    void updateItem(StructureModelItem* _item);

    /**
     * @brief Реализация древовидной модели
     */
    /** @{ */
    QModelIndex index(int _row, int _column, const QModelIndex& _parent = {}) const override;
    QModelIndex parent(const QModelIndex& _child) const override;
    int columnCount( const QModelIndex& _parent = {}) const override;
    int rowCount(const QModelIndex &_parent = {}) const override;
    Qt::ItemFlags flags(const QModelIndex &_index) const override;
    QVariant data(const QModelIndex &_index, int _role) const override;
    //! Реализация перетаскивания элементов
    bool canDropMimeData(const QMimeData* _data, Qt::DropAction _action, int _row, int _column, const QModelIndex& _parent = {}) const override;
    bool dropMimeData(const QMimeData* _data, Qt::DropAction _action, int _row, int _column, const QModelIndex& _parent = {}) override;
    QMimeData* mimeData(const QModelIndexList& _indexes) const override;
    QStringList mimeTypes() const override;
    Qt::DropActions supportedDragActions() const override;
    Qt::DropActions supportedDropActions() const override;
    /** @} */

    /**
     * @brief Получить элемент находящийся в заданном индексе
     */
    StructureModelItem* itemForIndex(const QModelIndex& _index) const;

    /**
     * @brief Задать имя элемента
     */
    void setItemName(const QModelIndex& _index, const QString& _name);

signals:
    /**
     * @brief Был добавлен документ с заданным идентификатором и типом
     */
    void documentAdded(const QUuid& _uuid, Domain::DocumentObjectType _type, const QString& _name);

protected:
    /**
     * @brief Реализация модели для работы с документами
     */
    /** @{ */
    void initDocument() override;
    void clearDocument() override;
    QByteArray toXml() const override;
    /** @} */

private:
    /**
     * @brief Получить индекс заданного элемента
     */
    QModelIndex indexForItem(StructureModelItem* _item) const;

private:
    class Implementation;
    QScopedPointer<Implementation> d;
};

} // namespace BusinessLayer
