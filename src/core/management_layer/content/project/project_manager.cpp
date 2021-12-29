#include "project_manager.h"

#include "project_models_facade.h"
#include "project_plugins_builder.h"

#include <business_layer/model/characters/character_model.h>
#include <business_layer/model/characters/characters_model.h>
#include <business_layer/model/comic_book/text/comic_book_text_model.h>
#include <business_layer/model/locations/location_model.h>
#include <business_layer/model/locations/locations_model.h>
#include <business_layer/model/project/project_information_model.h>
#include <business_layer/model/screenplay/text/screenplay_text_model.h>
#include <business_layer/model/structure/structure_model.h>
#include <business_layer/model/structure/structure_model_item.h>
#include <business_layer/model/structure/structure_proxy_model.h>
#include <data_layer/storage/document_change_storage.h>
#include <data_layer/storage/document_data_storage.h>
#include <data_layer/storage/document_storage.h>
#include <data_layer/storage/settings_storage.h>
#include <data_layer/storage/storage_facade.h>
#include <domain/document_change_object.h>
#include <domain/document_object.h>
#include <interfaces/management_layer/i_document_manager.h>
#include <interfaces/ui/i_document_view.h>
#include <ui/abstract_navigator.h>
#include <ui/design_system/design_system.h>
#include <ui/project/create_document_dialog.h>
#include <ui/project/project_navigator.h>
#include <ui/project/project_tool_bar.h>
#include <ui/project/project_view.h>
#include <ui/widgets/context_menu/context_menu.h>
#include <ui/widgets/dialog/dialog.h>

#include <QAction>
#include <QDateTime>
#include <QHBoxLayout>
#include <QSet>
#include <QUuid>
#include <QVariantAnimation>


namespace ManagementLayer {

class ProjectManager::Implementation
{
public:
    explicit Implementation(QWidget* _parent);

    /**
     * @brief Действия контекстного меню
     */
    enum class ContextMenuAction { AddDocument, RemoveDocument, EmptyRecycleBin };

    /**
     * @brief Обновить модель действий контекстного меню навигатора
     */
    void updateNavigatorContextMenu(const QModelIndex& _index);

    /**
     * @brief Добавить документ в проект
     */
    void addDocument();

    /**
     * @brief Добавить документ в заданный контейнер
     */
    void addDocumentToContainer(Domain::DocumentObjectType _containerType,
                                Domain::DocumentObjectType _documentType,
                                const QString& _documentName, const QByteArray& _content = {});

    /**
     * @brief Удалить документ
     */
    void removeDocument(const QModelIndex& _itemIndex);
    void removeDocument(BusinessLayer::StructureModelItem* _item);

    /**
     * @brief Найти всех персонажей
     */
    void findAllCharacters();

    /**
     * @brief Найти все локации
     */
    void findAllLocations();

    /**
     * @brief Очистить корзину
     */
    void emptyRecycleBin(const QModelIndex& _recycleBinIndex);

    //
    // Данные
    //

    QWidget* topLevelWidget = nullptr;

    Ui::ProjectToolBar* toolBar = nullptr;

    Ui::ProjectNavigator* navigator = nullptr;

    Ui::ProjectView* view = nullptr;

    BusinessLayer::StructureModel* projectStructureModel = nullptr;
    BusinessLayer::StructureProxyModel* projectStructureProxyModel = nullptr;

    DataStorageLayer::DocumentImageStorage documentDataStorage;

    ProjectModelsFacade modelsFacade;
    ProjectPluginsBuilder pluginsBuilder;

    /**
     * @brief Информация о текущем документе
     */
    struct {
        BusinessLayer::AbstractModel* model = nullptr;
        QString viewMimeType;
    } currentDocument;
};

ProjectManager::Implementation::Implementation(QWidget* _parent)
    : topLevelWidget(_parent)
    , toolBar(new Ui::ProjectToolBar(_parent))
    , navigator(new Ui::ProjectNavigator(_parent))
    , view(new Ui::ProjectView(_parent))
    , projectStructureModel(new BusinessLayer::StructureModel(navigator))
    , projectStructureProxyModel(new BusinessLayer::StructureProxyModel(projectStructureModel))
    , modelsFacade(projectStructureModel, &documentDataStorage)
{
    toolBar->hide();
    navigator->hide();
    view->hide();

    navigator->setModel(projectStructureProxyModel);
}

void ProjectManager::Implementation::updateNavigatorContextMenu(const QModelIndex& _index)
{
    QVector<QAction*> menuActions;

    const auto currentItemIndex = projectStructureProxyModel->mapToSource(_index);
    const auto currentItem = projectStructureModel->itemForIndex(currentItemIndex);

    //
    // Формируем список действий для конкретных элементов структуры проекта
    //
    if (currentItem->type() == Domain::DocumentObjectType::Characters) {
        auto findAllCharacters = new QAction(tr("Find all characters"));
        findAllCharacters->setIconText(u8"\U000F0016");
        connect(findAllCharacters, &QAction::triggered, [this] { this->findAllCharacters(); });
        menuActions.append(findAllCharacters);

        auto addCharacter = new QAction(tr("Add character"));
        addCharacter->setIconText(u8"\U000F0014");
        connect(addCharacter, &QAction::triggered, [this] { this->addDocument(); });
        menuActions.append(addCharacter);
    } else if (currentItem->type() == Domain::DocumentObjectType::Locations) {
        auto findAllLocations = new QAction(tr("Find all locations"));
        findAllLocations->setIconText(u8"\U000F13B0");
        connect(findAllLocations, &QAction::triggered, [this] { this->findAllLocations(); });
        menuActions.append(findAllLocations);

        auto addLocation = new QAction(tr("Add location"));
        addLocation->setIconText(u8"\U000F0975");
        connect(addLocation, &QAction::triggered, [this] { this->addDocument(); });
        menuActions.append(addLocation);
    } else if (currentItem->type() == Domain::DocumentObjectType::RecycleBin) {
        if (currentItem->hasChildren()) {
            auto emptyRecycleBin = new QAction(tr("Empty recycle bin"));
            emptyRecycleBin->setIconText(u8"\U000f05e8");
            connect(emptyRecycleBin, &QAction::triggered,
                    [this, currentItemIndex] { this->emptyRecycleBin(currentItemIndex); });
            menuActions.append(emptyRecycleBin);
        }
    }
    //
    // ... для остальных элементов
    //
    else {
        auto addDocument = new QAction(tr("Add document"));
        addDocument->setIconText(u8"\U000f0415");
        connect(addDocument, &QAction::triggered, [this] { this->addDocument(); });
        menuActions.append(addDocument);

        const QSet<Domain::DocumentObjectType> cantBeRemovedItems = {
            Domain::DocumentObjectType::Project,
            Domain::DocumentObjectType::Characters,
            Domain::DocumentObjectType::Locations,
            Domain::DocumentObjectType::ScreenplayTitlePage,
            Domain::DocumentObjectType::ScreenplaySynopsis,
            Domain::DocumentObjectType::ScreenplayTreatment,
            Domain::DocumentObjectType::ScreenplayText,
            Domain::DocumentObjectType::ScreenplayStatistics,
            Domain::DocumentObjectType::ComicBookTitlePage,
            Domain::DocumentObjectType::ComicBookSynopsis,
            Domain::DocumentObjectType::ComicBookText,
            Domain::DocumentObjectType::ComicBookStatistics,
        };
        if (_index.isValid() && !cantBeRemovedItems.contains(currentItem->type())) {
            auto removeDocument = new QAction(tr("Remove document"));
            removeDocument->setIconText(u8"\U000f01b4");
            connect(removeDocument, &QAction::triggered,
                    [this, currentItemIndex] { this->removeDocument(currentItemIndex); });
            menuActions.append(removeDocument);
        }
    }

    navigator->setContextMenuActions(menuActions);
}

void ProjectManager::Implementation::addDocument()
{
    const auto currentItemIndex
        = projectStructureProxyModel->mapToSource(navigator->currentIndex());
    const auto currentItem = projectStructureModel->itemForIndex(currentItemIndex);

    auto dialog = new Ui::CreateDocumentDialog(topLevelWidget);
    if (currentItem->type() == Domain::DocumentObjectType::Folder) {
        dialog->setInsertionParent(currentItem->name());
    } else if (currentItem->type() == Domain::DocumentObjectType::Characters) {
        dialog->setDocumentType(Domain::DocumentObjectType::Character);
    } else if (currentItem->type() == Domain::DocumentObjectType::Locations) {
        dialog->setDocumentType(Domain::DocumentObjectType::Location);
    }

    connect(
        dialog, &Ui::CreateDocumentDialog::createPressed, navigator,
        [this, currentItemIndex, dialog](Domain::DocumentObjectType _type, const QString& _name) {
            //
            // Определим индекс родительского элемента, ищем именно папку
            //
            auto parentIndex
                = dialog->needInsertIntoParent() ? currentItemIndex : currentItemIndex.parent();
            auto parentItem = projectStructureModel->itemForIndex(parentIndex);
            while (parentIndex.isValid() && parentItem != nullptr
                   && parentItem->type() != Domain::DocumentObjectType::Folder) {
                parentIndex = parentIndex.parent();
                parentItem = projectStructureModel->itemForIndex(parentIndex);
            }

            const auto addedItemIndex
                = projectStructureModel->addDocument(_type, _name, parentIndex);
            const auto mappedAddedItemIndex
                = projectStructureProxyModel->mapFromSource(addedItemIndex);
            navigator->setCurrentIndex(mappedAddedItemIndex);

            dialog->hideDialog();
        });
    connect(dialog, &Ui::CreateDocumentDialog::disappeared, dialog,
            &Ui::CreateDocumentDialog::deleteLater);

    dialog->showDialog();
}

void ProjectManager::Implementation::addDocumentToContainer(
    Domain::DocumentObjectType _containerType, Domain::DocumentObjectType _documentType,
    const QString& _documentName, const QByteArray& _content)
{
    for (int itemRow = 0; itemRow < projectStructureModel->rowCount(); ++itemRow) {
        const auto itemIndex = projectStructureModel->index(itemRow, 0);
        const auto item = projectStructureModel->itemForIndex(itemIndex);
        if (item->type() == _containerType) {
            projectStructureModel->addDocument(_documentType, _documentName, itemIndex, _content);
            break;
        }
    }
}

void ProjectManager::Implementation::removeDocument(const QModelIndex& _itemIndex)
{
    auto item = projectStructureModel->itemForIndex(_itemIndex);
    if (item == nullptr) {
        return;
    }

    removeDocument(item);
}

void ProjectManager::Implementation::removeDocument(BusinessLayer::StructureModelItem* _item)
{
    auto itemTopLevelParent = _item->parent();
    if (itemTopLevelParent == nullptr) {
        return;
    }
    while (itemTopLevelParent->parent()
           && itemTopLevelParent->parent()->type() != Domain::DocumentObjectType::Undefined) {
        itemTopLevelParent = itemTopLevelParent->parent();
    }

    //
    // Если документ ещё не в корзине, то переносим его в корзину
    //
    if (itemTopLevelParent->type() != Domain::DocumentObjectType::RecycleBin) {
        projectStructureModel->moveItemToRecycleBin(_item);
        return;
    }

    //
    // Если документ в корзине
    //
    // ... то спросим действительно ли пользователь хочет его удалить
    //
    const int kCancelButtonId = 0;
    const int kRemoveButtonId = 1;
    auto dialog = new Dialog(topLevelWidget);
    dialog->showDialog({}, tr("Do you really want to permanently remove document?"),
                       { { kCancelButtonId, tr("No"), Dialog::RejectButton },
                         { kRemoveButtonId, tr("Yes, remove"), Dialog::NormalButton } });
    QObject::connect(
        dialog, &Dialog::finished,
        [this, _item, kCancelButtonId, dialog](const Dialog::ButtonInfo& _buttonInfo) {
            dialog->hideDialog();

            //
            // Пользователь передумал удалять
            //
            if (_buttonInfo.id == kCancelButtonId) {
                return;
            }

            //
            // Если таки хочет, то удаляем документ
            // NOTE: порядок удаления важен
            //
            auto document
                = DataStorageLayer::StorageFacade::documentStorage()->document(_item->uuid());
            modelsFacade.removeModelFor(document);
            DataStorageLayer::StorageFacade::documentStorage()->removeDocument(document);
            projectStructureModel->removeItem(_item);
        });
    QObject::connect(dialog, &Dialog::disappeared, dialog, &Dialog::deleteLater);
}

void ProjectManager::Implementation::findAllCharacters()
{
    //
    // Найти все модели где могут встречаться персонажи и определить их
    //
    QSet<QString> charactersFromText;
    const auto screenplayModels
        = modelsFacade.modelsFor(Domain::DocumentObjectType::ScreenplayText);
    for (auto model : screenplayModels) {
        auto screenplay = qobject_cast<BusinessLayer::ScreenplayTextModel*>(model);
        charactersFromText.unite(screenplay->findCharactersFromText());
    }
    //
    const auto comicBookModels = modelsFacade.modelsFor(Domain::DocumentObjectType::ComicBookText);
    for (auto model : comicBookModels) {
        auto comicBook = qobject_cast<BusinessLayer::ComicBookTextModel*>(model);
        charactersFromText.unite(comicBook->findCharactersFromText());
    }
    charactersFromText.remove({});

    //
    // Определить персонажи, которых нет в тексте
    //
    QSet<QString> charactersNotFromText;
    const auto charactersModel = qobject_cast<BusinessLayer::CharactersModel*>(
        modelsFacade.modelFor(Domain::DocumentObjectType::Characters));
    for (int row = 0; row < charactersModel->rowCount(); ++row) {
        const auto characterName = charactersModel->index(row, 0).data().toString();
        if (!charactersFromText.contains(characterName)) {
            charactersNotFromText.insert(characterName);
        }
    }

    //
    // Спросить пользователя, что он хочет сделать с ними
    //
    QString message;
    if (!charactersFromText.isEmpty()) {
        QStringList characters = charactersFromText.values();
        std::sort(characters.begin(), characters.end());
        message.append(
            QString("%1:\n%2.").arg(tr("Characters from the text"), characters.join(", ")));
    }
    if (!charactersNotFromText.isEmpty()) {
        if (!message.isEmpty()) {
            message.append("\n\n");
        }
        QStringList characters = charactersNotFromText.values();
        std::sort(characters.begin(), characters.end());
        message.append(
            QString("%1:\n%2.")
                .arg(tr("Characters that are not found in the text"), characters.join(", ")));
    }
    const int kCancelButtonId = 0;
    const int kKeepFromTextButtonId = 1;
    const int kKeepAllButtonId = 2;
    auto dialog = new Dialog(topLevelWidget);
    //
    // Т.к. персов может быть очень много, расширяем максимальное ограничение по ширине для диалога
    //
    dialog->setContentMaximumWidth(topLevelWidget->width() * 0.7);
    const auto placeButtonSideBySide = false;
    dialog->showDialog(
        {}, message,
        { { kKeepFromTextButtonId, tr("Save only characters from the text"), Dialog::NormalButton },
          { kKeepAllButtonId, tr("Save all characters"), Dialog::NormalButton },
          { kCancelButtonId, tr("Change nothing"), Dialog::RejectButton } },
        placeButtonSideBySide);
    QObject::connect(
        dialog, &Dialog::finished, dialog,
        [this, charactersFromText, charactersNotFromText, charactersModel, dialog, kCancelButtonId,
         kKeepFromTextButtonId](const Dialog::ButtonInfo& _buttonInfo) {
            dialog->hideDialog();

            //
            // Пользователь передумал что-либо менять
            //
            if (_buttonInfo.id == kCancelButtonId) {
                return;
            }

            //
            // Если надо, удалим персонажей, которые не встречаются в тексте
            //
            if (_buttonInfo.id == kKeepFromTextButtonId) {
                for (const auto& characterName : charactersNotFromText) {
                    const auto characterModel = charactersModel->character(characterName);
                    auto item
                        = projectStructureModel->itemForUuid(characterModel->document()->uuid());
                    removeDocument(item);
                }
            }

            //
            // Сохраняем новых персонажей, которых ещё не было в базе
            //
            for (const auto& characterName : charactersFromText) {
                if (charactersModel->exists(characterName)) {
                    continue;
                }

                addDocumentToContainer(Domain::DocumentObjectType::Characters,
                                       Domain::DocumentObjectType::Character, characterName);
            }
        });
    QObject::connect(dialog, &Dialog::disappeared, dialog, &Dialog::deleteLater);
}

void ProjectManager::Implementation::findAllLocations()
{
    //
    // Найти все модели где могут встречаться локации и определить их
    //
    QSet<QString> locationsFromText;
    const auto screenplayModels
        = modelsFacade.modelsFor(Domain::DocumentObjectType::ScreenplayText);
    for (auto model : screenplayModels) {
        auto screenplay = qobject_cast<BusinessLayer::ScreenplayTextModel*>(model);
        locationsFromText.unite(screenplay->findLocationsFromText());
    }
    locationsFromText.remove({});

    //
    // Определить локации, которых нет в тексте
    //
    QSet<QString> locationsNotFromText;
    const auto locationsModel = qobject_cast<BusinessLayer::LocationsModel*>(
        modelsFacade.modelFor(Domain::DocumentObjectType::Locations));
    for (int row = 0; row < locationsModel->rowCount(); ++row) {
        const auto locationName = locationsModel->index(row, 0).data().toString();
        if (!locationsFromText.contains(locationName)) {
            locationsNotFromText.insert(locationName);
        }
    }

    //
    // Спросить пользователя, что он хочет сделать с ними
    //
    QString message;
    if (!locationsFromText.isEmpty()) {
        QStringList locations = locationsFromText.values();
        std::sort(locations.begin(), locations.end());
        message.append(
            QString("%1:\n%2.").arg(tr("Locations from the text"), locations.join(", ")));
    }
    if (!locationsNotFromText.isEmpty()) {
        if (!message.isEmpty()) {
            message.append("\n\n");
        }
        QStringList locations = locationsNotFromText.values();
        std::sort(locations.begin(), locations.end());
        message.append(
            QString("%1:\n%2.")
                .arg(tr("Locations that are not found in the text"), locations.join(", ")));
    }
    const int kCancelButtonId = 0;
    const int kKeepFromTextButtonId = 1;
    const int kKeepAllButtonId = 2;
    auto dialog = new Dialog(topLevelWidget);
    //
    // Т.к. локаций может быть очень много, расширяем максимальное ограничение по ширине для диалога
    //
    dialog->setContentMaximumWidth(topLevelWidget->width() * 0.7);
    const auto placeButtonSideBySide = false;
    dialog->showDialog(
        {}, message,
        { { kKeepFromTextButtonId, tr("Save only locations from the text"), Dialog::NormalButton },
          { kKeepAllButtonId, tr("Save all locations"), Dialog::NormalButton },
          { kCancelButtonId, tr("Change nothing"), Dialog::RejectButton } },
        placeButtonSideBySide);
    QObject::connect(
        dialog, &Dialog::finished, dialog,
        [this, locationsFromText, locationsNotFromText, locationsModel, dialog, kCancelButtonId,
         kKeepFromTextButtonId](const Dialog::ButtonInfo& _buttonInfo) {
            dialog->hideDialog();

            //
            // Пользователь передумал что-либо менять
            //
            if (_buttonInfo.id == kCancelButtonId) {
                return;
            }

            //
            // Если надо, удалим локации, которые не встречаются в тексте
            //
            if (_buttonInfo.id == kKeepFromTextButtonId) {
                for (const auto& locationName : locationsNotFromText) {
                    const auto locationModel = locationsModel->location(locationName);
                    auto item
                        = projectStructureModel->itemForUuid(locationModel->document()->uuid());
                    removeDocument(item);
                }
            }

            //
            // Сохраняем новых локации, которых ещё не было в базе
            //
            for (const auto& locationName : locationsFromText) {
                if (locationsModel->exists(locationName)) {
                    continue;
                }

                addDocumentToContainer(Domain::DocumentObjectType::Locations,
                                       Domain::DocumentObjectType::Location, locationName);
            }
        });
    QObject::connect(dialog, &Dialog::disappeared, dialog, &Dialog::deleteLater);
}

void ProjectManager::Implementation::emptyRecycleBin(const QModelIndex& _recycleBinIndex)
{
    auto recycleBin = projectStructureModel->itemForIndex(_recycleBinIndex);
    if (recycleBin == nullptr) {
        return;
    }

    //
    // Спросим действительно ли пользователь хочет очистить корзину
    //
    const int kCancelButtonId = 0;
    const int kEmptyButtonId = 1;
    auto dialog = new Dialog(topLevelWidget);
    dialog->showDialog(
        {}, tr("Do you really want to permanently remove all documents from the recycle bin?"),
        { { kCancelButtonId, tr("No"), Dialog::RejectButton },
          { kEmptyButtonId, tr("Yes, remove"), Dialog::NormalButton } });
    QObject::connect(
        dialog, &Dialog::finished,
        [this, recycleBin, kCancelButtonId, dialog](const Dialog::ButtonInfo& _buttonInfo) {
            dialog->hideDialog();

            //
            // Пользователь передумал очищать корзину
            //
            if (_buttonInfo.id == kCancelButtonId) {
                return;
            }

            //
            // Если таки хочет, то удаляем все вложенные документы рекурсивно
            // NOTE: порядок удаления важен
            //
            std::function<void(BusinessLayer::StructureModelItem*)> removeItem;
            removeItem = [this, &removeItem](BusinessLayer::StructureModelItem* _item) {
                //
                // Сначала удаляем детей
                //
                while (_item->hasChildren()) {
                    auto child = _item->childAt(0);
                    removeItem(child);
                }
                //
                // ... а потом сам элемент
                //
                auto documentToRemove
                    = DataStorageLayer::StorageFacade::documentStorage()->document(_item->uuid());
                if (documentToRemove != nullptr) {
                    modelsFacade.removeModelFor(documentToRemove);
                    DataStorageLayer::StorageFacade::documentStorage()->removeDocument(
                        documentToRemove);
                }
                projectStructureModel->removeItem(_item);
            };
            while (recycleBin->hasChildren()) {
                auto itemToRemove = recycleBin->childAt(0);
                removeItem(itemToRemove);
            }
        });
    QObject::connect(dialog, &Dialog::disappeared, dialog, &Dialog::deleteLater);
}


// ****


ProjectManager::ProjectManager(QObject* _parent, QWidget* _parentWidget)
    : QObject(_parent)
    , d(new Implementation(_parentWidget))
{
    connect(d->toolBar, &Ui::ProjectToolBar::menuPressed, this, &ProjectManager::menuRequested);
    connect(d->toolBar, &Ui::ProjectToolBar::viewPressed, this, [this](const QString& _mimeType) {
        showView(d->navigator->currentIndex(), _mimeType);
    });

    //
    // Отображаем необходимый редактор при выборе документа в списке
    //
    connect(d->navigator, &Ui::ProjectNavigator::itemSelected, this,
            [this](const QModelIndex& _index) {
                if (!_index.isValid()) {
                    d->view->showDefaultPage();
                    return;
                }

                //
                // Определим выделенный элемент и скорректируем интерфейс
                //
                const auto mappedIndex = d->projectStructureProxyModel->mapToSource(_index);
                const auto item = d->projectStructureModel->itemForIndex(mappedIndex);
                const auto documentMimeType = Domain::mimeTypeFor(item->type());
                //
                // ... настроим иконки представлений
                //
                d->toolBar->clearViews();
                const auto views = d->pluginsBuilder.editorsInfoFor(documentMimeType);
                for (const auto& view : views) {
                    const auto tooltip
                        = d->pluginsBuilder.editorDescription(documentMimeType, view.mimeType);
                    const bool isActive = view.mimeType == views.first().mimeType;
                    d->toolBar->addView(view.mimeType, view.icon, tooltip, isActive);
                }

                //
                // Откроем документ на редактирование в первом из представлений
                //
                if (views.isEmpty()) {
                    d->view->showNotImplementedPage();
                    return;
                }
                showView(_index, views.first().mimeType);
            });
    //
    // Отображаем навигатор выбранного элемента
    //
    connect(d->navigator, &Ui::ProjectNavigator::itemDoubleClicked, this,
            [this](const QModelIndex& _index) {
                const auto mappedIndex = d->projectStructureProxyModel->mapToSource(_index);
                if (!d->projectStructureModel
                         ->data(mappedIndex,
                                static_cast<int>(
                                    BusinessLayer::StructureModelDataRole::IsNavigatorAvailable))
                         .toBool()) {
                    return;
                }

                //                if (mappedIndex.child())
                showNavigator(_index);
            });
    connect(d->navigator, &Ui::ProjectNavigator::itemNavigationRequested, this,
            [this](const QModelIndex& _index) { showNavigator(_index); });
    connect(d->navigator, &Ui::ProjectNavigator::contextMenuUpdateRequested, this,
            [this](const QModelIndex& _index) { d->updateNavigatorContextMenu(_index); });
    connect(d->navigator, &Ui::ProjectNavigator::addDocumentClicked, this,
            [this] { d->addDocument(); });

    //
    // Соединения с моделью структуры проекта
    //
    connect(d->projectStructureModel, &BusinessLayer::StructureModel::documentAdded,
            [this](const QUuid& _uuid, const QUuid& _parentUuid, Domain::DocumentObjectType _type,
                   const QString& _name, const QByteArray& _content) {
                Q_UNUSED(_parentUuid);

                auto document = DataStorageLayer::StorageFacade::documentStorage()->createDocument(
                    _uuid, _type);
                if (!_content.isNull()) {
                    document->setContent(_content);
                }

                auto documentModel = d->modelsFacade.modelFor(document);
                documentModel->setDocumentName(_name);

                switch (_type) {
                case Domain::DocumentObjectType::Character: {
                    auto charactersDocument
                        = DataStorageLayer::StorageFacade::documentStorage()->document(
                            Domain::DocumentObjectType::Characters);
                    auto charactersModel = qobject_cast<BusinessLayer::CharactersModel*>(
                        d->modelsFacade.modelFor(charactersDocument));
                    auto characterModel
                        = qobject_cast<BusinessLayer::CharacterModel*>(documentModel);
                    charactersModel->addCharacterModel(characterModel);

                    break;
                }

                case Domain::DocumentObjectType::Location: {
                    auto locationsDocument
                        = DataStorageLayer::StorageFacade::documentStorage()->document(
                            Domain::DocumentObjectType::Locations);
                    auto locationsModel = qobject_cast<BusinessLayer::LocationsModel*>(
                        d->modelsFacade.modelFor(locationsDocument));
                    auto locationModel = qobject_cast<BusinessLayer::LocationModel*>(documentModel);
                    locationsModel->addLocationModel(locationModel);

                    break;
                }

                default:
                    break;
                }
            });
    connect(d->projectStructureModel, &BusinessLayer::StructureModel::contentsChanged, this,
            [this](const QByteArray& _undo, const QByteArray& _redo) {
                handleModelChange(d->projectStructureModel, _undo, _redo);
            });
    connect(
        d->projectStructureModel, &BusinessLayer::StructureModel::rowsAboutToBeMoved, this,
        [this](const QModelIndex& _sourceParent, int _sourceStart, int _sourceEnd,
               const QModelIndex& _destination) {
            const auto sourceParentItem = d->projectStructureModel->itemForIndex(_sourceParent);
            const auto destinationItem = d->projectStructureModel->itemForIndex(_destination);
            if (sourceParentItem == nullptr || destinationItem == nullptr) {
                return;
            }

            //
            // Удаляем персонажа
            //
            if (sourceParentItem->type() == Domain::DocumentObjectType::Characters
                && destinationItem->type() == Domain::DocumentObjectType::RecycleBin) {
                const auto charactersDocument
                    = DataStorageLayer::StorageFacade::documentStorage()->document(
                        Domain::DocumentObjectType::Characters);
                auto charactersModel = d->modelsFacade.modelFor(charactersDocument);
                auto characters = qobject_cast<BusinessLayer::CharactersModel*>(charactersModel);
                for (int row = _sourceStart; row <= _sourceEnd; ++row) {
                    const auto removedItemIndex
                        = d->projectStructureModel->index(row, 0, _sourceParent);
                    const auto removedItem
                        = d->projectStructureModel->itemForIndex(removedItemIndex);
                    characters->removeCharacterModel(qobject_cast<BusinessLayer::CharacterModel*>(
                        d->modelsFacade.modelFor(removedItem->uuid())));
                }
            }
            //
            // Удаляем локации
            //
            else if (sourceParentItem->type() == Domain::DocumentObjectType::Locations
                     && destinationItem->type() == Domain::DocumentObjectType::RecycleBin) {
                const auto locationsDocument
                    = DataStorageLayer::StorageFacade::documentStorage()->document(
                        Domain::DocumentObjectType::Locations);
                auto locationsModel = d->modelsFacade.modelFor(locationsDocument);
                auto locations = qobject_cast<BusinessLayer::LocationsModel*>(locationsModel);
                for (int row = _sourceStart; row <= _sourceEnd; ++row) {
                    const auto removedItemIndex
                        = d->projectStructureModel->index(row, 0, _sourceParent);
                    const auto removedItem
                        = d->projectStructureModel->itemForIndex(removedItemIndex);
                    locations->removeLocationModel(qobject_cast<BusinessLayer::LocationModel*>(
                        d->modelsFacade.modelFor(removedItem->uuid())));
                }
            }
            //
            // Восстанавливаем персонажей
            //
            else if (sourceParentItem->type() == Domain::DocumentObjectType::RecycleBin
                     && destinationItem->type() == Domain::DocumentObjectType::Characters) {
                const auto charactersDocument
                    = DataStorageLayer::StorageFacade::documentStorage()->document(
                        Domain::DocumentObjectType::Characters);
                auto charactersModel = d->modelsFacade.modelFor(charactersDocument);
                auto characters = qobject_cast<BusinessLayer::CharactersModel*>(charactersModel);
                for (int row = _sourceStart; row <= _sourceEnd; ++row) {
                    const auto removedItemIndex
                        = d->projectStructureModel->index(row, 0, _sourceParent);
                    const auto removedItem
                        = d->projectStructureModel->itemForIndex(removedItemIndex);
                    characters->addCharacterModel(qobject_cast<BusinessLayer::CharacterModel*>(
                        d->modelsFacade.modelFor(removedItem->uuid())));
                }
            }
            //
            // Восстанавливаем локации
            //
            else if (sourceParentItem->type() == Domain::DocumentObjectType::RecycleBin
                     && destinationItem->type() == Domain::DocumentObjectType::Locations) {
                const auto locationsDocument
                    = DataStorageLayer::StorageFacade::documentStorage()->document(
                        Domain::DocumentObjectType::Locations);
                auto locationsModel = d->modelsFacade.modelFor(locationsDocument);
                auto locations = qobject_cast<BusinessLayer::LocationsModel*>(locationsModel);
                for (int row = _sourceStart; row <= _sourceEnd; ++row) {
                    const auto removedItemIndex
                        = d->projectStructureModel->index(row, 0, _sourceParent);
                    const auto removedItem
                        = d->projectStructureModel->itemForIndex(removedItemIndex);
                    locations->addLocationModel(qobject_cast<BusinessLayer::LocationModel*>(
                        d->modelsFacade.modelFor(removedItem->uuid())));
                }
            }
        });

    //
    // Соединения представления
    //
    connect(d->view, &Ui::ProjectView::createNewItemPressed, this, [this] { d->addDocument(); });

    //
    // Соединения со строителем моделей
    //
    connect(&d->modelsFacade, &ProjectModelsFacade::modelNameChanged, this,
            [this](BusinessLayer::AbstractModel* _model, const QString& _name) {
                auto item = d->projectStructureModel->itemForUuid(_model->document()->uuid());
                d->projectStructureModel->setItemName(item, _name);
            });
    connect(&d->modelsFacade, &ProjectModelsFacade::modelColorChanged, this,
            [this](BusinessLayer::AbstractModel* _model, const QColor& _color) {
                auto item = d->projectStructureModel->itemForUuid(_model->document()->uuid());
                d->projectStructureModel->setItemColor(item, _color);
            });
    connect(&d->modelsFacade, &ProjectModelsFacade::modelContentChanged, this,
            &ProjectManager::handleModelChange);
    connect(&d->modelsFacade, &ProjectModelsFacade::modelUndoRequested, this,
            &ProjectManager::undoModelChange);
    connect(&d->modelsFacade, &ProjectModelsFacade::modelRemoveRequested, this,
            [this](BusinessLayer::AbstractModel* _model) {
                auto item = d->projectStructureModel->itemForUuid(_model->document()->uuid());
                d->removeDocument(item);
            });
    connect(&d->modelsFacade, &ProjectModelsFacade::projectNameChanged, this,
            &ProjectManager::projectNameChanged);
    connect(&d->modelsFacade, &ProjectModelsFacade::projectLoglineChanged, this,
            &ProjectManager::projectLoglineChanged);
    connect(&d->modelsFacade, &ProjectModelsFacade::projectCoverChanged, this,
            &ProjectManager::projectCoverChanged);
    connect(&d->modelsFacade, &ProjectModelsFacade::createCharacterRequested, this,
            [this](const QString& _name, const QByteArray& _content) {
                d->addDocumentToContainer(Domain::DocumentObjectType::Characters,
                                          Domain::DocumentObjectType::Character, _name, _content);
            });
    connect(&d->modelsFacade, &ProjectModelsFacade::characterNameChanged, this,
            [this](const QString& _newName, const QString& _oldName) {
                //
                // Найти все модели где может встречаться персонаж и заменить в них его имя со
                // старого на новое
                //
                const auto screenplayModels
                    = d->modelsFacade.modelsFor(Domain::DocumentObjectType::ScreenplayText);
                for (auto model : screenplayModels) {
                    auto screenplay = qobject_cast<BusinessLayer::ScreenplayTextModel*>(model);
                    screenplay->updateCharacterName(_oldName, _newName);
                }
                //
                const auto comicBookModels
                    = d->modelsFacade.modelsFor(Domain::DocumentObjectType::ComicBookText);
                for (auto model : comicBookModels) {
                    auto comicBook = qobject_cast<BusinessLayer::ComicBookTextModel*>(model);
                    comicBook->updateCharacterName(_oldName, _newName);
                }
            });
    connect(&d->modelsFacade, &ProjectModelsFacade::createLocationRequested, this,
            [this](const QString& _name, const QByteArray& _content) {
                d->addDocumentToContainer(Domain::DocumentObjectType::Locations,
                                          Domain::DocumentObjectType::Location, _name, _content);
            });
    connect(&d->modelsFacade, &ProjectModelsFacade::locationNameChanged, this,
            [this](const QString& _newName, const QString& _oldName) {
                //
                // Найти все модели где может встречаться персонаж и заменить в них его имя со
                // старого на новое
                //
                const auto models
                    = d->modelsFacade.modelsFor(Domain::DocumentObjectType::ScreenplayText);
                for (auto model : models) {
                    auto screenplay = qobject_cast<BusinessLayer::ScreenplayTextModel*>(model);
                    screenplay->updateLocationName(_oldName, _newName);
                }
            });
    //
    auto setDocumentVisible = [this](BusinessLayer::AbstractModel* _screenplayModel,
                                     Domain::DocumentObjectType _type, bool _visible) {
        auto screenplayItem
            = d->projectStructureModel->itemForUuid(_screenplayModel->document()->uuid());
        for (int childIndex = 0; childIndex < screenplayItem->childCount(); ++childIndex) {
            auto childItem = screenplayItem->childAt(childIndex);
            if (childItem->type() == _type) {
                d->projectStructureModel->setItemVisible(childItem, _visible);
                break;
            }
        }
    };
    connect(&d->modelsFacade, &ProjectModelsFacade::screenplayTitlePageVisibilityChanged, this,
            [setDocumentVisible](BusinessLayer::AbstractModel* _screenplayModel, bool _visible) {
                setDocumentVisible(_screenplayModel,
                                   Domain::DocumentObjectType::ScreenplayTitlePage, _visible);
            });
    connect(&d->modelsFacade, &ProjectModelsFacade::screenplaySynopsisVisibilityChanged, this,
            [setDocumentVisible](BusinessLayer::AbstractModel* _screenplayModel, bool _visible) {
                setDocumentVisible(_screenplayModel, Domain::DocumentObjectType::ScreenplaySynopsis,
                                   _visible);
            });
    connect(&d->modelsFacade, &ProjectModelsFacade::screenplayTreatmentVisibilityChanged, this,
            [setDocumentVisible](BusinessLayer::AbstractModel* _screenplayModel, bool _visible) {
                setDocumentVisible(_screenplayModel,
                                   Domain::DocumentObjectType::ScreenplayTreatment, _visible);
            });
    connect(&d->modelsFacade, &ProjectModelsFacade::screenplayTextVisibilityChanged, this,
            [setDocumentVisible](BusinessLayer::AbstractModel* _screenplayModel, bool _visible) {
                setDocumentVisible(_screenplayModel, Domain::DocumentObjectType::ScreenplayText,
                                   _visible);
            });
    connect(&d->modelsFacade, &ProjectModelsFacade::screenplayStatisticsVisibilityChanged, this,
            [setDocumentVisible](BusinessLayer::AbstractModel* _screenplayModel, bool _visible) {
                setDocumentVisible(_screenplayModel,
                                   Domain::DocumentObjectType::ScreenplayStatistics, _visible);
            });
    connect(&d->modelsFacade, &ProjectModelsFacade::comicBookTitlePageVisibilityChanged, this,
            [setDocumentVisible](BusinessLayer::AbstractModel* _comicBookModel, bool _visible) {
                setDocumentVisible(_comicBookModel, Domain::DocumentObjectType::ComicBookTitlePage,
                                   _visible);
            });
    connect(&d->modelsFacade, &ProjectModelsFacade::comicBookSynopsisVisibilityChanged, this,
            [setDocumentVisible](BusinessLayer::AbstractModel* _comicBookModel, bool _visible) {
                setDocumentVisible(_comicBookModel, Domain::DocumentObjectType::ComicBookSynopsis,
                                   _visible);
            });
    connect(&d->modelsFacade, &ProjectModelsFacade::comicBookTextVisibilityChanged, this,
            [setDocumentVisible](BusinessLayer::AbstractModel* _comicBookModel, bool _visible) {
                setDocumentVisible(_comicBookModel, Domain::DocumentObjectType::ComicBookText,
                                   _visible);
            });
    connect(&d->modelsFacade, &ProjectModelsFacade::comicBookStatisticsVisibilityChanged, this,
            [setDocumentVisible](BusinessLayer::AbstractModel* _comicBookModel, bool _visible) {
                setDocumentVisible(_comicBookModel, Domain::DocumentObjectType::ComicBookStatistics,
                                   _visible);
            });
}

ProjectManager::~ProjectManager() = default;

QWidget* ProjectManager::toolBar() const
{
    return d->toolBar;
}

QWidget* ProjectManager::navigator() const
{
    return d->navigator;
}

QWidget* ProjectManager::view() const
{
    return d->view;
}

void ProjectManager::toggleFullScreen(bool _isFullScreen)
{
    d->pluginsBuilder.toggleFullScreen(_isFullScreen, d->currentDocument.viewMimeType);
}

void ProjectManager::reconfigureAll()
{
    d->pluginsBuilder.reconfigureAll();
}

void ProjectManager::reconfigureSimpleTextEditor(const QStringList& _changedSettingsKeys)
{
    d->pluginsBuilder.reconfigureSimpleTextEditor(_changedSettingsKeys);
}

void ProjectManager::reconfigureSimpleTextNavigator()
{
    d->pluginsBuilder.reconfigureSimpleTextNavigator();
}

void ProjectManager::reconfigureScreenplayEditor(const QStringList& _changedSettingsKeys)
{
    d->pluginsBuilder.reconfigureScreenplayEditor(_changedSettingsKeys);
}

void ProjectManager::reconfigureScreenplayNavigator()
{
    d->pluginsBuilder.reconfigureScreenplayNavigator();
}

void ProjectManager::reconfigureScreenplayDuration()
{
    for (auto model : d->modelsFacade.loadedModels()) {
        auto screenplayModel = qobject_cast<BusinessLayer::ScreenplayTextModel*>(model);
        if (screenplayModel == nullptr) {
            continue;
        }

        screenplayModel->recalculateDuration();
    }
}

void ProjectManager::reconfigureComicBookEditor(const QStringList& _changedSettingsKeys)
{
    d->pluginsBuilder.reconfigureComicBookEditor(_changedSettingsKeys);
}

void ProjectManager::reconfigureComicBookNavigator()
{
    d->pluginsBuilder.reconfigureComicBookNavigator();
}

void ProjectManager::checkAvailabilityToEdit()
{
    d->pluginsBuilder.checkAvailabilityToEdit();
}

void ProjectManager::loadCurrentProject(const QString& _name, const QString& _path)
{
    //
    // Загружаем структуру
    //
    d->projectStructureModel->setProjectName(_name);
    d->projectStructureModel->setDocument(
        DataStorageLayer::StorageFacade::documentStorage()->document(
            Domain::DocumentObjectType::Structure));

    //
    // Загружаем информацию о проекте
    //
    auto projectInformationModel = qobject_cast<BusinessLayer::ProjectInformationModel*>(
        d->modelsFacade.modelFor(DataStorageLayer::StorageFacade::documentStorage()->document(
            Domain::DocumentObjectType::Project)));
    if (projectInformationModel->name().isEmpty()) {
        projectInformationModel->setName(_name);
    } else {
        emit projectNameChanged(projectInformationModel->name());
        emit projectLoglineChanged(projectInformationModel->logline());
        emit projectCoverChanged(projectInformationModel->cover());
    }


    //
    // Синхронизировать структуру с облаком
    //

    //
    // Открыть структуру
    //

    //
    // Загрузить состояние дерева
    //
    d->navigator->restoreState(d->projectStructureModel->isNewProject(),
                               settingsValue(DataStorageLayer::projectStructureKey(_path)));

    //
    // При необходимости открыть навигатор по документу
    //
    const auto isProjectStructureVisible
        = settingsValue(DataStorageLayer::projectStructureVisibleKey(_path));
    if (isProjectStructureVisible.isValid() && !isProjectStructureVisible.toBool()) {
        showNavigator(d->navigator->currentIndex());
    }

    //
    // Синхронизировать выбранный документ
    //

    //
    // Синхрониировать все остальные изменения
    //
}

void ProjectManager::closeCurrentProject(const QString& _path)
{
    //
    // Сохранить состояние дерева
    //
    setSettingsValue(DataStorageLayer::projectStructureKey(_path), d->navigator->saveState());
    setSettingsValue(DataStorageLayer::projectStructureVisibleKey(_path),
                     d->navigator->isProjectNavigatorShown());

    //
    // Очищаем структуру
    //
    d->projectStructureModel->clear();

    //
    // Сбрасываем все плагины
    //
    d->pluginsBuilder.resetModels();

    //
    // Очищаем все загруженные модели документов
    //
    d->modelsFacade.clear();

    //
    // Сбрасываем загруженные изображения
    //
    d->documentDataStorage.clear();
}

void ProjectManager::saveChanges()
{
    //
    // Сохраняем структуру
    //
    const auto structure = d->projectStructureModel->document();
    DataStorageLayer::StorageFacade::documentStorage()->saveDocument(structure);

    //
    // Сохраняем остальные документы
    //
    for (auto model : d->modelsFacade.loadedModels()) {
        DataStorageLayer::StorageFacade::documentStorage()->saveDocument(model->document());
    }

    //
    // Сохраняем изображения
    //
    d->documentDataStorage.saveChanges();

    //
    // Сохраняем все изменения документов
    //
    DataStorageLayer::StorageFacade::documentChangeStorage()->store();
}

void ProjectManager::addCharacter(const QString& _name, const QString& _content)
{
    auto document = DataStorageLayer::StorageFacade::documentStorage()->document(
        Domain::DocumentObjectType::Characters);
    auto model = d->modelsFacade.modelFor(document);
    auto charactersModel = qobject_cast<BusinessLayer::CharactersModel*>(model);
    if (charactersModel == nullptr) {
        return;
    }

    charactersModel->createCharacter(_name, _content.toUtf8());
}

void ProjectManager::addLocation(const QString& _name, const QString& _content)
{
    auto document = DataStorageLayer::StorageFacade::documentStorage()->document(
        Domain::DocumentObjectType::Locations);
    auto model = d->modelsFacade.modelFor(document);
    auto locationsModel = qobject_cast<BusinessLayer::LocationsModel*>(model);
    if (locationsModel == nullptr) {
        return;
    }

    locationsModel->createLocation(_name, _content.toUtf8());
}

void ProjectManager::addScreenplay(const QString& _name, const QString& _titlePage,
                                   const QString& _synopsis, const QString& _treatment,
                                   const QString& _text)
{
    //
    // ATTENTION: Копипаста из StructureModel::addDocument, быть внимательным при обновлении
    //

    using namespace Domain;

    auto createItem = [](DocumentObjectType _type, const QString& _name) {
        auto uuid = QUuid::createUuid();
        const auto visible = true;
        return new BusinessLayer::StructureModelItem(uuid, _type, _name, {}, visible);
    };

    auto rootItem = d->projectStructureModel->itemForIndex({});
    auto screenplayItem = createItem(DocumentObjectType::Screenplay, _name);
    d->projectStructureModel->appendItem(screenplayItem, rootItem);

    d->projectStructureModel->appendItem(
        createItem(DocumentObjectType::ScreenplayTitlePage, tr("Title page")), screenplayItem,
        _titlePage.toUtf8());
    d->projectStructureModel->appendItem(
        createItem(DocumentObjectType::ScreenplaySynopsis, tr("Synopsis")), screenplayItem,
        _synopsis.toUtf8());
    d->projectStructureModel->appendItem(
        createItem(DocumentObjectType::ScreenplayTreatment, tr("Treatment")), screenplayItem,
        _treatment.toUtf8());
    d->projectStructureModel->appendItem(
        createItem(DocumentObjectType::ScreenplayText, tr("Screenplay")), screenplayItem,
        _text.toUtf8());
    d->projectStructureModel->appendItem(
        createItem(DocumentObjectType::ScreenplayStatistics, tr("Statistics")), screenplayItem, {});
}

BusinessLayer::AbstractModel* ProjectManager::currentModel() const
{
    return d->currentDocument.model;
}

void ProjectManager::handleModelChange(BusinessLayer::AbstractModel* _model,
                                       const QByteArray& _undo, const QByteArray& _redo)
{
    using namespace DataStorageLayer;
    StorageFacade::documentChangeStorage()->appendDocumentChange(
        _model->document()->uuid(), QUuid::createUuid(), _undo, _redo,
        StorageFacade::settingsStorage()->accountName(),
        StorageFacade::settingsStorage()->accountEmail());

    emit contentsChanged();
}

void ProjectManager::undoModelChange(BusinessLayer::AbstractModel* _model, int _undoStep)
{
    const auto change = DataStorageLayer::StorageFacade::documentChangeStorage()->documentChangeAt(
        _model->document()->uuid(), _undoStep);
    if (change == nullptr) {
        return;
    }

    _model->undoChange(change->undoPatch(), change->redoPatch());
}

void ProjectManager::showView(const QModelIndex& _itemIndex, const QString& _viewMimeType)
{
    if (!_itemIndex.isValid()) {
        updateCurrentDocument(nullptr, {});
        d->view->showDefaultPage();
        return;
    }

    const auto mappedItemIndex = d->projectStructureProxyModel->mapToSource(_itemIndex);
    const auto item = d->projectStructureModel->itemForIndex(mappedItemIndex);

    //
    // Определим модель
    //
    updateCurrentDocument(d->modelsFacade.modelFor(item->uuid()), _viewMimeType);
    if (d->currentDocument.model == nullptr) {
        d->view->showNotImplementedPage();
        return;
    }

    //
    // Определим представление и отобразим
    //
    auto view = d->pluginsBuilder.activateView(_viewMimeType, d->currentDocument.model);
    if (view == nullptr) {
        d->view->showNotImplementedPage();
        return;
    }
    d->view->setCurrentWidget(view->asQWidget());

    //
    // Настроим возможность перехода в навигатор
    //
    const auto navigatorMimeType = d->pluginsBuilder.navigatorMimeTypeFor(_viewMimeType);
    d->projectStructureModel->setNavigatorAvailableFor(mappedItemIndex,
                                                       !navigatorMimeType.isEmpty());

    //
    // Если в данный момент отображён кастомный навигатов, откроем навигатор соответствующий
    // редактору
    //
    if (!d->navigator->isProjectNavigatorShown()) {
        showNavigator(_itemIndex, _viewMimeType);
    }

    //
    // Настроим уведомления плагина
    //
    auto documentManager = d->pluginsBuilder.plugin(_viewMimeType)->asQObject();
    if (documentManager) {
        const auto invalidSignalIndex = -1;
        if (documentManager->metaObject()->indexOfSignal("upgradeRequested()")
            != invalidSignalIndex) {
            connect(documentManager, SIGNAL(upgradeRequested()), this, SIGNAL(upgradeRequested()),
                    Qt::UniqueConnection);
        }
    }
}

void ProjectManager::showNavigator(const QModelIndex& _itemIndex, const QString& _viewMimeType)
{
    const auto mappedItemIndex = d->projectStructureProxyModel->mapToSource(_itemIndex);
    if (!mappedItemIndex.isValid()) {
        d->navigator->showProjectNavigator();
        return;
    }

    const auto item = d->projectStructureModel->itemForIndex(mappedItemIndex);

    //
    // Определим модель
    //
    auto model = d->modelsFacade.modelFor(item->uuid());
    if (model == nullptr) {
        d->navigator->showProjectNavigator();
        return;
    }

    //
    // Определим представление и отобразим
    //
    const auto viewMimeType
        = !_viewMimeType.isEmpty() ? _viewMimeType : d->toolBar->currentViewMimeType();
    const auto navigatorMimeType = d->pluginsBuilder.navigatorMimeTypeFor(viewMimeType);
    auto view = d->pluginsBuilder.activateView(navigatorMimeType, model);
    if (view == nullptr) {
        d->navigator->showProjectNavigator();
        return;
    }

    //
    // Связываем редактор с навигатоом
    //
    d->pluginsBuilder.bind(viewMimeType, navigatorMimeType);

    //
    // Настраиваем возможность перехода к навигатору проекта
    //
    auto navigatorView = qobject_cast<Ui::AbstractNavigator*>(view->asQWidget());
    connect(navigatorView, &Ui::AbstractNavigator::backPressed, d->navigator,
            &Ui::ProjectNavigator::showProjectNavigator, Qt::UniqueConnection);
    d->navigator->setCurrentWidget(navigatorView);
}

void ProjectManager::updateCurrentDocument(BusinessLayer::AbstractModel* _model,
                                           const QString& _viewMimeType)
{
    d->currentDocument.model = _model;
    d->currentDocument.viewMimeType = _viewMimeType;

    emit currentModelChanged(d->currentDocument.model);
}

} // namespace ManagementLayer
