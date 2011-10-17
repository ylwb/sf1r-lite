/**
 * @file ItemFilter.h
 * @author Jun Jiang
 * @date 2011-05-03
 */

#ifndef ITEM_FILTER_H
#define ITEM_FILTER_H

#include <idmlib/resys/ItemRescorer.h>
#include "RecTypes.h"
#include "ItemManager.h"
#include "ItemCondition.h"
#include "RecommendParam.h"

#include <vector>

namespace sf1r
{

class ItemFilter : public idmlib::recommender::ItemRescorer
{
public:
    ItemFilter(
        ItemManager& itemManager,
        const RecommendParam& param
    )
        : itemManager_(itemManager)
        , condition_(param.condition)
    {
        insert(param.includeItemIds.begin(), param.includeItemIds.end());
        insert(param.excludeItemIds.begin(), param.excludeItemIds.end());
    }

    float rescore(itemid_t itemId, float originalScore)
    {
        return 0;
    }

    /**
     * Check whether to filter this item.
     * @param itemId the item id to check
     * @return true to filter @p itemId, false to assume @p itemId as recommendation candidate.
     */
    bool isFiltered(itemid_t itemId)
    {
        // check filter set first
        if (filterSet_.find(itemId) != filterSet_.end())
            return true;

        // no condition
        if (condition_.propName_.empty())
            return itemManager_.hasItem(itemId) == false;

        // not exist
        Item item;
        if (itemManager_.getItem(itemId, item) == false)
            return true;

        return condition_.checkItem(item) == false;
    }

    /**
     * Insert the item id to filter.
     * @param itemId the item id to filter
     */
    void insert(itemid_t itemId)
    {
        filterSet_.insert(itemId);
    }

    /**
     * Insert the item ids in the range [first, last) to filter.
     * @param first the start of item range, including this item
     * @param last the end of item range, excluding this item
     */
    template<class InputIterator> void insert(InputIterator first, InputIterator last)
    {
        filterSet_.insert(first, last);
    }

private:
    ItemManager& itemManager_;
    const ItemCondition& condition_;
    std::set<itemid_t> filterSet_;
};

} // namespace sf1r

#endif // ITEM_FILTER_H
