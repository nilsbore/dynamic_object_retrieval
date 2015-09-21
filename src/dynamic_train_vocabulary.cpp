#include "dynamic_object_retrieval/summary_types.h"
#include "dynamic_object_retrieval/summary_iterators.h"
#include "dynamic_object_retrieval/visualize.h"

#include <vocabulary_tree/vocabulary_tree.h>
#include <grouped_vocabulary_tree/grouped_vocabulary_tree.h>

#include <cereal/archives/binary.hpp>
#include <pcl/common/centroid.h>

using namespace std;
using namespace dynamic_object_retrieval;

using PointT = pcl::PointXYZRGB;
using CloudT = pcl::PointCloud<PointT>;
using HistT = pcl::Histogram<250>;
using HistCloudT = pcl::PointCloud<HistT>;

POINT_CLOUD_REGISTER_POINT_STRUCT (HistT,
                                   (float[250], histogram, histogram)
)

// TODO: finish this
set<pair<int, int> > compute_group_adjacencies(CloudT::Ptr& centroids, float adj_dist)
{
    set<pair<int, int> > adjacencies;

    for (int i = 0; i < centroids->size(); ++i) {
        for (int j = 0; j < i; ++j) {
            if ((centroids->at(i).getVector3fMap()-centroids->at(j).getVector3fMap()).norm() < adj_dist) {
                adjacencies.insert(make_pair(i, j));
                // adjacencies.insert(make_pair(j, i));
            }
        }
    }
}

template <typename SegmentMapT>
size_t add_segments(SegmentMapT& segment_features, const boost::filesystem::path& vocabulary_path,
                   const vocabulary_summary& summary, bool training, size_t offset)
{
    size_t min_segment_features = summary.min_segment_features;
    size_t max_training_features = summary.max_training_features;
    size_t max_append_features = summary.max_append_features;

    vocabulary_tree<HistT, 8> vt;

    if (!training) {
        load_vocabulary(vt, vocabulary_path);
    }

    HistCloudT::Ptr features(new HistCloudT);
    vector<int> indices;

    size_t counter = 0;
    // add an iterator with the segment nbr???
    for (HistCloudT::Ptr features_i : segment_features) {

        // train on a subset of the provided features
        if (training && features->size() > max_training_features) {
            vt.set_input_cloud(features, indices);
            vt.add_points_from_input_cloud(false);
            features->clear();
            indices.clear();
            training = false;
        }

        if (!training && features->size() > max_append_features) {
            vt.append_cloud(features, indices, false);
            features->clear();
            indices.clear();
        }

        if (features_i->size() < min_segment_features) {
            ++counter;
            continue;
        }
        features->insert(features->end(), features_i->begin(), features_i->end());
        for (size_t i = 0; i < features_i->size(); ++i) {
            indices.push_back(offset + counter);
        }

        ++counter;
    }

    // append the rest
    vt.append_cloud(features, indices, false);
    save_vocabulary(vt, vocabulary_path);

    return counter;
}

// with some small differences, this could be used instead of the first one
// add VocabularyT::index_type in the classes
// also, add an iterator that returns segment and sweep index for segments
template <typename VocabularyT, typename SegmentMapT, typename KeypointMapT, typename SweepMapT>
pair<size_t, size_t> add_segments_grouped(SegmentMapT& segment_features, KeypointMapT& segment_keypoints,
                                          SweepMapT& sweep_indices, const boost::filesystem::path& vocabulary_path,
                                          const vocabulary_summary& summary, bool training,
                                          size_t segment_offset, size_t sweep_offset)
{
    size_t min_segment_features = summary.min_segment_features;
    size_t max_training_features = summary.max_training_features;
    size_t max_append_features = summary.max_append_features;

    VocabularyT vt("root_path");

    if (!training) {
        load_vocabulary(vt, vocabulary_path);
    }

    HistCloudT::Ptr features(new HistCloudT);
    CloudT::Ptr centroids(new CloudT);
    vector<set<pair<int, int> > > adjacencies;
    vector<pair<int, int> > indices;
    // VocabularyT::indices_type; !!!!!!!!!!!!

    size_t counter = 0;
    size_t sweep_i;
    size_t last_sweep = 0;
    // add an iterator with the segment nbr??? maybe not
    // but! add an iterator with the sweep nbr!
    for (auto tup : zip(segment_features, segment_keypoints, sweep_indices)) {

        HistCloudT::Ptr features_i;
        CloudT::Ptr keypoints_i;
        tie(features_i, keypoints_i, sweep_i) = tup;

        // train on a subset of the provided features
        if (sweep_i != last_sweep) {

            adjacencies.push_back(compute_group_adjacencies(centroids, 0.3f));
            centroids->clear();

            if (training && features->size() > max_training_features) {
                vt.set_input_cloud(features, indices);
                vt.add_points_from_input_cloud(adjacencies, false);
                features->clear();
                indices.clear();
                adjacencies.clear();
                training = false;
            }

            if (!training && features->size() > max_append_features) {
                vt.append_cloud(features, indices, adjacencies, false);
                features->clear();
                indices.clear();
                adjacencies.clear();
            }

            last_sweep = sweep_i;
        }

        if (features_i->size() < min_segment_features) {
            ++counter;
            continue;
        }

        Eigen::Vector4f point;
        pcl::compute3DCentroid(*keypoints_i, point);
        centroids->back().getVector4fMap() = point;
        features->insert(features->end(), features_i->begin(), features_i->end());
        pair<int, int> index(segment_offset + counter, sweep_offset + sweep_i);
        for (size_t i = 0; i < features_i->size(); ++i) {
            indices.push_back(index);
        }

        ++counter;
    }

    // append the rest
    vt.append_cloud(features, indices, false);
    save_vocabulary(vt, vocabulary_path);

    return make_pair(counter, sweep_i + 1);
}

void train_vocabulary(const boost::filesystem::path& vocabulary_path)
{
    vocabulary_summary summary;
    summary.load(vocabulary_path);

    boost::filesystem::path noise_data_path = summary.noise_data_path;
    boost::filesystem::path annotated_data_path = summary.annotated_data_path;

    if (summary.vocabulary_type == "standard") {
        convex_feature_cloud_map noise_segment_features(noise_data_path);
        convex_feature_cloud_map annotated_segment_features(annotated_data_path);
        summary.nbr_noise_segments = add_segments(noise_segment_features, vocabulary_path, summary, true, 0);
        summary.nbr_annotated_segments = add_segments(annotated_segment_features, vocabulary_path, summary, false, summary.nbr_noise_segments);
    }
    else if (summary.vocabulary_type == "incremental") {
        subsegment_feature_cloud_map noise_segment_features(noise_data_path);
        subsegment_feature_cloud_map annotated_segment_features(annotated_data_path);
        subsegment_cloud_map noise_segment_keypoints(noise_data_path);
        subsegment_cloud_map annotated_segment_keypoints(annotated_data_path);
        subsegment_sweep_index_map noise_sweep_indices(noise_data_path);
        subsegment_sweep_index_map annotated_sweep_indices(annotated_data_path);
        tie(summary.nbr_noise_segments, summary.nbr_noise_sweeps) =
                add_segments_grouped<grouped_vocabulary_tree<HistT, 8> >(
                    noise_segment_features, noise_segment_keypoints, noise_sweep_indices,
                    vocabulary_path, summary, true, 0, 0);
        tie(summary.nbr_annotated_segments, summary.nbr_annotated_sweeps) =
                add_segments_grouped<grouped_vocabulary_tree<HistT, 8> >(
                    annotated_segment_features, annotated_segment_keypoints, annotated_sweep_indices, vocabulary_path,
                    summary, false, summary.nbr_noise_segments, summary.nbr_noise_sweeps);
    }

    summary.save(vocabulary_path);
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        cout << "Please supply the path containing the vocabulary..." << endl;
        return 0;
    }

    train_vocabulary(boost::filesystem::path(argv[1]));

    return 0;
}