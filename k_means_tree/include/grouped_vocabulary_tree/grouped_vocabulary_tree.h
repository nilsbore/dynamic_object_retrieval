#ifndef GROUPED_VOCABULARY_TREE_H
#define GROUPED_VOCABULARY_TREE_H

#include "vocabulary_tree/vocabulary_tree.h"

#include <unordered_map>
#include <vector>
#include <map>

#include <cereal/types/unordered_map.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/types/set.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include <cereal/archives/binary.hpp>

/*
 * grouped_vocabulary_tree
 *
 * This class describes a collection of points that occurr together
 *
 */

// we need these two initial indices to denote the indices
// of the group and the index within the group
struct grouped_result : public vocabulary_result {
    int group_index; // the index among all the other groups
    int subgroup_index; // the index within this subgroup
    std::vector<int> subgroup_global_indices; // the indices of the within all of the segments
    std::vector<int> subgroup_group_indices; // the indices of the subsegments within this group
    grouped_result(float score, int group_index, int subgroup_index) :
        vocabulary_result(0, score), group_index(group_index), subgroup_index(subgroup_index),
        subgroup_global_indices(0), subgroup_group_indices(0) {}
    grouped_result() : vocabulary_result() {}
};

template <typename Point, size_t K>
class grouped_vocabulary_tree : public vocabulary_tree<Point, K> {
protected:

    using super = vocabulary_tree<Point, K>;
    using typename super::leaf;
    using typename super::PointT;
    using typename super::CloudT;
    using typename super::CloudPtrT;
    using typename super::leaf_range;

public:

    using typename super::node;
    using result_type = grouped_result; //std::tuple<int, int, double>;
    using group_type = std::vector<int>;
    using index_type = std::tuple<int, int, int>;

public: // protected:

    // maps from global index to (group index, index within group)
    std::unordered_map<int, std::pair<int, int> > group_subgroup;
    size_t nbr_points;
    size_t nbr_subgroups;

protected:

    // TODO: check if any of these are necessary, clean up this mess of a class!
    std::string save_state_path;
    std::map<node*, int> mapping; // for mapping to unique node IDs that can be used in the next run, might be empty

protected:

    // for caching the vocabulary vectors
    void cache_group_adjacencies(int start_ind, std::vector<std::set<std::pair<int, int> > >& adjacencies);
    void cache_vocabulary_vectors(int start_ind, CloudPtrT& cloud);
    void save_cached_vocabulary_vectors_for_group(std::vector<vocabulary_vector>& vectors, int i);

public:

    // should maybe be protected but needed for incremental segmentation comparison
    void load_cached_vocabulary_vectors_for_group(std::vector<vocabulary_vector>& vectors, std::set<std::pair<int, int> >& adjacencies, int i);

    void query_vocabulary(std::vector<result_type>& results, CloudPtrT& query_cloud, size_t nbr_query);

    void get_subgroups_for_group(std::set<int>& subgroups, int group_id);
    int get_id_for_group_subgroup(int group_id, int subgroup_id);

    //void set_input_cloud(CloudPtrT& new_cloud, std::vector<std::pair<int, int> >& indices);
    void set_input_cloud(CloudPtrT& new_cloud, std::vector<index_type>& indices);
    //void append_cloud(CloudPtrT& extra_cloud, std::vector<std::pair<int, int> >& indices, bool store_points = true);
    void append_cloud(CloudPtrT& extra_cloud, std::vector<index_type>& indices, bool store_points = true);
    //void append_cloud(CloudPtrT& extra_cloud, std::vector<std::pair<int, int> >& indices, std::vector<std::set<std::pair<int, int> > >& adjacencies, bool store_points = true);
    void append_cloud(CloudPtrT& extra_cloud, std::vector<index_type>& indices, std::vector<std::set<std::pair<int, int> > >& adjacencies, bool store_points = true);
    void add_points_from_input_cloud(bool save_cloud = true);
    void add_points_from_input_cloud(std::vector<std::set<std::pair<int, int> > >& adjacencies, bool save_cloud = true);
    void top_combined_similarities(std::vector<result_type>& scores, CloudPtrT& query_cloud, size_t nbr_results);

    void set_cache_path(const std::string& cache_path)
    {
        save_state_path = cache_path;
    }

    void clear()
    {
        // here we just need to remember that we also need to either remove the
        // cached vocabulary vectors or save the cleared (go L Ron) vocabularies somewhere else
        super::clear();
        group_subgroup.clear();
        nbr_points = 0;
        nbr_subgroups = 0;
    }

    /*
    template <class Archive> void save(Archive& archive) const;
    template <class Archive> void load(Archive& archive);
    */
    template <class Archive>
    void load(Archive& archive)
    {
        super::load(archive);
        archive(nbr_points, nbr_subgroups, group_subgroup, save_state_path);
        std::cout << "Finished loading grouped_vocabulary_tree" << std::endl;
    }

    template <class Archive>
    void save(Archive& archive) const
    {
        super::save(archive);
        archive(nbr_points, nbr_subgroups, group_subgroup, save_state_path);
    }

    grouped_vocabulary_tree() : super(), nbr_points(0), nbr_subgroups(0) {}
    grouped_vocabulary_tree(const std::string& save_state_path) : super(), nbr_points(0), nbr_subgroups(0), save_state_path(save_state_path) {}
};

#ifndef VT_PRECOMPILE
#include "grouped_vocabulary_tree.hpp"
#endif

#endif // GROUPED_VOCABULARY_TREE_H
