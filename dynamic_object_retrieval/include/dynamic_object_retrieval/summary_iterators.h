#ifndef SUMMARY_ITERATORS_H
#define SUMMARY_ITERATORS_H

#include <boost/filesystem.hpp>
#include <boost/iterator/zip_iterator.hpp>
#include <boost/range.hpp>

#include <metaroom_xml_parser/load_utilities.h>

#include "dynamic_object_retrieval/summary_types.h"
#include "dynamic_object_retrieval/definitions.h"

/*
 * For iterating over the segmentations that are produced by the method
 *
 */

using PointT = pcl::PointXYZRGB;
using HistT = pcl::Histogram<N>;

namespace dynamic_object_retrieval {

inline boost::filesystem::path get_sweep_xml(size_t sweep_id, const vocabulary_summary& summary)
{
    static std::vector<std::string> noise_xmls;
    static std::vector<std::string> annotated_xmls;

    std::cout << "Looking a sweep_id: " << sweep_id << std::endl;
    std::cout << "And number of noise sweeps: " << summary.nbr_noise_sweeps << std::endl;
    if (sweep_id < summary.nbr_noise_sweeps) {
        if (noise_xmls.empty()) {
            noise_xmls = semantic_map_load_utilties::getSweepXmls<PointT>(summary.noise_data_path);
        }
        return boost::filesystem::path(noise_xmls[sweep_id]).parent_path();
    }
    else {
        if (annotated_xmls.empty()) {
            annotated_xmls = semantic_map_load_utilties::getSweepXmls<PointT>(summary.annotated_data_path);
        }
        sweep_id -= summary.nbr_noise_sweeps;
        return boost::filesystem::path(annotated_xmls[sweep_id]).parent_path();
    }
    //std::cout << "Number of xmls: " << xmls.size() << std::endl;
    //std::cout << "Index: " << sweep_id << std::endl;
    //return boost::filesystem::path(xmls[sweep_id]).parent_path();
}

struct segment_iterator_base {

    // for iterating sweeps
    std::vector<std::string> folder_xmls; // very easy to just swap this for different values
    std::string folder_name; // either "convex_segments" or "subsegments"
    std::string segment_name; // so that we can iterate over both "segment" and "pfhrgbfeature"
    size_t xml_pos;
    size_t xml_size;
    //std::vector<std::string>::iterator xml_iter;
    //std::vector<std::string>::iterator end_iter;
    boost::filesystem::path current_path;
    size_t current_nbr_segments;
    size_t current_segment;

    // for iterating
    bool operator!= (const segment_iterator_base& other) const // we basically make this a synonyme for at_end
    {
        //return xml_iter != end_iter;
        return xml_pos < xml_size;
    }
    bool operator== (const segment_iterator_base& other) const // we basically make this a synonyme for at_end
    {
        //return xml_iter == end_iter;
        return xml_pos >= xml_size;
    }
    void load_next_summary()
    {
        //if (xml_iter != end_iter) {
        if (xml_pos < xml_size) {
            std::cout << "Analyzing sweep: " << folder_xmls[xml_pos] << std::endl;
            //current_path = boost::filesystem::path(*xml_iter).parent_path() / folder_name;
            current_path = boost::filesystem::path(folder_xmls[xml_pos]).parent_path() / folder_name;
            sweep_summary summary;
            summary.load(current_path);
            current_nbr_segments = summary.nbr_segments;
            current_segment = 0;
        }
    }
    void operator++ ()
    {
        ++current_segment;
        if (current_segment >= current_nbr_segments) {
            //++xml_iter;
            ++xml_pos;
            load_next_summary();
        }
    }
    segment_iterator_base(const std::vector<std::string>& xmls,
                          const std::string& folder_name,
                          const std::string& segment_name) :
        folder_xmls(xmls), folder_name(folder_name), segment_name(segment_name),
        xml_pos(0), xml_size(folder_xmls.size())
        //xml_iter(folder_xmls.begin()), end_iter(folder_xmls.end()), temp(0)
    {
        load_next_summary();
    }
    segment_iterator_base() {} // for the end() method
};

struct segment_path_iterator : public segment_iterator_base, public std::iterator<std::forward_iterator_tag, boost::filesystem::path> {

    mutable boost::filesystem::path current_value;

    boost::filesystem::path& operator* () const
    {
        std::stringstream ss;
        ss << segment_name << std::setw(4) << std::setfill('0') << current_segment;
        current_value = current_path / (ss.str() + ".pcd");
        return current_value;
    }

    segment_path_iterator(const std::vector<std::string>& xmls,
                           const std::string& folder_name,
                           const std::string& segment_name) :
        segment_iterator_base(xmls, folder_name, segment_name)
    {

    }
    segment_path_iterator() : segment_iterator_base() {}
};

struct segment_sweep_path_iterator : public segment_iterator_base, public std::iterator<std::forward_iterator_tag, boost::filesystem::path> {

    mutable boost::filesystem::path current_value;

    boost::filesystem::path& operator* () const
    {
        current_value = current_path.parent_path();
        return current_value;
    }

    segment_sweep_path_iterator(const std::vector<std::string>& xmls,
                                const std::string& folder_name,
                                const std::string& segment_name) :
        segment_iterator_base(xmls, folder_name, segment_name)
    {

    }
    segment_sweep_path_iterator() : segment_iterator_base() {}
};

struct segment_sweep_index_iterator : public segment_iterator_base, public std::iterator<std::forward_iterator_tag, size_t> {

    mutable size_t current_value;
    mutable bool first;

    size_t& operator* () const
    {
        if (!first && current_segment == 0) {
            ++current_value;
        }
        first = false;
        return current_value;
    }

    segment_sweep_index_iterator(const std::vector<std::string>& xmls,
                                 const std::string& folder_name,
                                 const std::string& segment_name) :
        segment_iterator_base(xmls, folder_name, segment_name), current_value(0), first(true)
    {

    }
    segment_sweep_index_iterator() : segment_iterator_base() {}
};

struct segment_index_iterator : public segment_iterator_base, public std::iterator<std::forward_iterator_tag, size_t> {

    mutable size_t current_value;

    size_t& operator* () const
    {
        current_value = current_segment;
        return current_value;
    }

    segment_index_iterator(const std::vector<std::string>& xmls,
                           const std::string& folder_name,
                           const std::string& segment_name) :
        segment_iterator_base(xmls, folder_name, segment_name), current_value(0)
    {

    }
    segment_index_iterator() : segment_iterator_base() {}
};

struct sweep_segment_index_iterator : public segment_iterator_base, public std::iterator<std::forward_iterator_tag, size_t> {

    mutable size_t current_value;
    mutable std::vector<int> segment_indices;

    size_t& operator* () const
    {
        if (current_segment == 0) {
            sweep_summary summary;
            summary.load(current_path);
            segment_indices = summary.segment_indices;
        }
        current_value = segment_indices[current_segment];
        return current_value;
    }

    sweep_segment_index_iterator(const std::vector<std::string>& xmls,
                                 const std::string& folder_name,
                                 const std::string& segment_name) :
        segment_iterator_base(xmls, folder_name, segment_name), current_value(0)
    {

    }
    sweep_segment_index_iterator() : segment_iterator_base() {}
};

template <typename CloudT>
struct segment_cloud_iterator : public segment_iterator_base, public std::iterator<std::forward_iterator_tag, typename CloudT::Ptr> {

    using CloudPtrT = typename CloudT::Ptr;

    mutable CloudPtrT current_value;

    CloudPtrT& operator* () const
    {
        std::stringstream ss;
        ss << segment_name << std::setw(4) << std::setfill('0') << current_segment;
        boost::filesystem::path cloud_path = current_path / (ss.str() + ".pcd");
        pcl::io::loadPCDFile(cloud_path.string(), *current_value);
        return current_value;
    }

    segment_cloud_iterator(const std::vector<std::string>& xmls,
                           const std::string& folder_name,
                           const std::string& segment_name) :
        segment_iterator_base(xmls, folder_name, segment_name), current_value(new CloudT)
    {

    }
    segment_cloud_iterator() : segment_iterator_base() {}
};

struct islast_segment_iterator : public segment_iterator_base, public std::iterator<std::forward_iterator_tag, bool> {

    mutable bool current_value;

    bool& operator* () const
    {
        current_value = current_segment >= current_nbr_segments - 1;
        return current_value;
    }

    islast_segment_iterator(const std::vector<std::string>& xmls,
                            const std::string& folder_name,
                            const std::string& segment_name) :
        segment_iterator_base(xmls, folder_name, segment_name), current_value(false)
    {

    }
    islast_segment_iterator() : segment_iterator_base() {}
};

// iterators over all convex segments

struct convex_sweep_index_map {

    boost::filesystem::path data_path;

    segment_sweep_index_iterator begin()
    {
        return segment_sweep_index_iterator(semantic_map_load_utilties::getSweepXmls<PointT>(data_path.string()), "convex_segments", "segment");
    }

    segment_sweep_index_iterator end()
    {
        return segment_sweep_index_iterator();
    }

    convex_sweep_index_map(const boost::filesystem::path& data_path) : data_path(data_path) {}
};

struct convex_segment_sweep_path_map {

    boost::filesystem::path data_path;

    segment_sweep_path_iterator begin()
    {
        return segment_sweep_path_iterator(semantic_map_load_utilties::getSweepXmls<PointT>(data_path.string()), "convex_segments", "segment");
    }

    segment_sweep_path_iterator end()
    {
        return segment_sweep_path_iterator();
    }

    convex_segment_sweep_path_map(const boost::filesystem::path& data_path) : data_path(data_path) {}
};

struct convex_segment_index_map {

    boost::filesystem::path data_path;

    segment_index_iterator begin()
    {
        return segment_index_iterator(semantic_map_load_utilties::getSweepXmls<PointT>(data_path.string()), "convex_segments", "segment");
    }

    segment_index_iterator end()
    {
        return segment_index_iterator();
    }

    convex_segment_index_map(const boost::filesystem::path& data_path) : data_path(data_path) {}
};

struct convex_segment_map {

    boost::filesystem::path data_path;

    segment_path_iterator begin()
    {
        return segment_path_iterator(semantic_map_load_utilties::getSweepXmls<PointT>(data_path.string()), "convex_segments", "segment");
    }

    segment_path_iterator end()
    {
        return segment_path_iterator();
    }

    convex_segment_map(const boost::filesystem::path& data_path) : data_path(data_path) {}
};

struct convex_feature_map {

    boost::filesystem::path data_path;

    segment_path_iterator begin()
    {
        return segment_path_iterator(semantic_map_load_utilties::getSweepXmls<PointT>(data_path.string()), "convex_segments", "pfhrgbfeature");
    }

    segment_path_iterator end()
    {
        return segment_path_iterator();
    }

    convex_feature_map(const boost::filesystem::path& data_path) : data_path(data_path) {}
};

struct convex_keypoint_map {

    boost::filesystem::path data_path;

    segment_path_iterator begin()
    {
        return segment_path_iterator(semantic_map_load_utilties::getSweepXmls<PointT>(data_path.string()), "convex_segments", "pfhrgbkeypoint");
    }

    segment_path_iterator end()
    {
        return segment_path_iterator();
    }

    convex_keypoint_map(const boost::filesystem::path& data_path) : data_path(data_path) {}
};

struct convex_segment_cloud_map {

    using iterator = segment_cloud_iterator<pcl::PointCloud<PointT> >;

    boost::filesystem::path data_path;

    iterator begin()
    {
        return iterator(semantic_map_load_utilties::getSweepXmls<PointT>(data_path.string()), "convex_segments", "segment");
    }

    iterator end()
    {
        return iterator();
    }

    convex_segment_cloud_map(const boost::filesystem::path& data_path) : data_path(data_path) {}
};

struct convex_feature_cloud_map {

    using iterator = segment_cloud_iterator<pcl::PointCloud<HistT> >;

    boost::filesystem::path data_path;

    iterator begin()
    {
        return iterator(semantic_map_load_utilties::getSweepXmls<PointT>(data_path.string()), "convex_segments", "pfhrgbfeature");
    }

    iterator end()
    {
        return iterator();
    }

    convex_feature_cloud_map(const boost::filesystem::path& data_path) : data_path(data_path) {}
};

struct convex_keypoint_cloud_map {

    using iterator = segment_cloud_iterator<pcl::PointCloud<PointT> >;

    boost::filesystem::path data_path;

    iterator begin()
    {
        return iterator(semantic_map_load_utilties::getSweepXmls<PointT>(data_path.string()), "convex_segments", "pfhrgbkeypoint");
    }

    iterator end()
    {
        return iterator();
    }

    convex_keypoint_cloud_map(const boost::filesystem::path& data_path) : data_path(data_path) {}
};

struct islast_convex_segment_map {

    boost::filesystem::path data_path;

    islast_segment_iterator begin()
    {
        return islast_segment_iterator(semantic_map_load_utilties::getSweepXmls<PointT>(data_path.string()), "convex_segments", "segment");
    }

    islast_segment_iterator end()
    {
        return islast_segment_iterator();
    }

    islast_convex_segment_map(const boost::filesystem::path& data_path) : data_path(data_path) {}
};

// iterators over all subsegments

struct subsegment_sweep_index_map {

    boost::filesystem::path data_path;

    segment_sweep_index_iterator begin()
    {
        return segment_sweep_index_iterator(semantic_map_load_utilties::getSweepXmls<PointT>(data_path.string()), "subsegments", "segment");
    }

    segment_sweep_index_iterator end()
    {
        return segment_sweep_index_iterator();
    }

    subsegment_sweep_index_map(const boost::filesystem::path& data_path) : data_path(data_path) {}
};

struct subsegment_sweep_path_map {

    boost::filesystem::path data_path;

    segment_sweep_path_iterator begin()
    {
        return segment_sweep_path_iterator(semantic_map_load_utilties::getSweepXmls<PointT>(data_path.string()), "subsegments", "segment");
    }

    segment_sweep_path_iterator end()
    {
        return segment_sweep_path_iterator();
    }

    subsegment_sweep_path_map(const boost::filesystem::path& data_path) : data_path(data_path) {}
};

struct subsegment_index_map {

    boost::filesystem::path data_path;

    segment_index_iterator begin()
    {
        return segment_index_iterator(semantic_map_load_utilties::getSweepXmls<PointT>(data_path.string()), "subsegments", "segment");
    }

    segment_index_iterator end()
    {
        return segment_index_iterator();
    }

    subsegment_index_map(const boost::filesystem::path& data_path) : data_path(data_path) {}
};

struct subsegment_map {

    boost::filesystem::path data_path;

    segment_path_iterator begin()
    {
        return segment_path_iterator(semantic_map_load_utilties::getSweepXmls<PointT>(data_path.string()), "subsegments", "segment");
    }

    segment_path_iterator end()
    {
        return segment_path_iterator();
    }

    subsegment_map(const boost::filesystem::path& data_path) : data_path(data_path) {}
};

struct subsegment_cloud_map {

    using iterator = segment_cloud_iterator<pcl::PointCloud<PointT> >;

    boost::filesystem::path data_path;

    iterator begin()
    {
        return iterator(semantic_map_load_utilties::getSweepXmls<PointT>(data_path.string()), "subsegments", "segment");
    }

    iterator end()
    {
        return iterator();
    }

    subsegment_cloud_map(const boost::filesystem::path& data_path) : data_path(data_path) {}
};

struct subsegment_feature_map {

    boost::filesystem::path data_path;

    segment_path_iterator begin()
    {
        return segment_path_iterator(semantic_map_load_utilties::getSweepXmls<PointT>(data_path.string()), "subsegments", "pfhrgbfeature");
    }

    segment_path_iterator end()
    {
        return segment_path_iterator();
    }

    subsegment_feature_map(const boost::filesystem::path& data_path) : data_path(data_path) {}
};

struct subsegment_keypoint_map {

    boost::filesystem::path data_path;

    segment_path_iterator begin()
    {
        return segment_path_iterator(semantic_map_load_utilties::getSweepXmls<PointT>(data_path.string()), "subsegments", "pfhrgbkeypoint");
    }

    segment_path_iterator end()
    {
        return segment_path_iterator();
    }

    subsegment_keypoint_map(const boost::filesystem::path& data_path) : data_path(data_path) {}
};

struct subsegment_feature_cloud_map {

    using iterator = segment_cloud_iterator<pcl::PointCloud<HistT> >;

    boost::filesystem::path data_path;

    iterator begin()
    {
        return iterator(semantic_map_load_utilties::getSweepXmls<PointT>(data_path.string()), "subsegments", "pfhrgbfeature");
    }

    iterator end()
    {
        return iterator();
    }

    subsegment_feature_cloud_map(const boost::filesystem::path& data_path) : data_path(data_path) {}
};

struct subsegment_keypoint_cloud_map {

    using iterator = segment_cloud_iterator<pcl::PointCloud<PointT> >;

    boost::filesystem::path data_path;

    iterator begin()
    {
        return iterator(semantic_map_load_utilties::getSweepXmls<PointT>(data_path.string()), "subsegments", "pfhrgbkeypoint");
    }

    iterator end()
    {
        return iterator();
    }

    subsegment_keypoint_cloud_map(const boost::filesystem::path& data_path) : data_path(data_path) {}
};

// iterators over convex segments of one sweep

struct sweep_convex_segment_map {

    boost::filesystem::path sweep_path;

    segment_path_iterator begin()
    {
        return segment_path_iterator(std::vector<std::string>({sweep_path.string()}), "convex_segments", "segment");
    }

    segment_path_iterator end()
    {
        return segment_path_iterator();
    }

    sweep_convex_segment_map(const boost::filesystem::path& sweep_path) : sweep_path(sweep_path / "room.xml") {}
};

struct sweep_convex_feature_map {

    boost::filesystem::path sweep_path;

    segment_path_iterator begin()
    {
        return segment_path_iterator(std::vector<std::string>({sweep_path.string()}), "convex_segments", "pfhrgbfeature");
    }

    segment_path_iterator end()
    {
        return segment_path_iterator();
    }

    sweep_convex_feature_map(const boost::filesystem::path& sweep_path) : sweep_path(sweep_path / "room.xml") {}
};

struct sweep_convex_keypoint_map {

    boost::filesystem::path sweep_path;

    segment_path_iterator begin()
    {
        return segment_path_iterator(std::vector<std::string>({sweep_path.string()}), "convex_segments", "pfhrgbkeypoint");
    }

    segment_path_iterator end()
    {
        return segment_path_iterator();
    }

    sweep_convex_keypoint_map(const boost::filesystem::path& sweep_path) : sweep_path(sweep_path / "room.xml") {}
};

struct sweep_convex_segment_cloud_map {

    using iterator = segment_cloud_iterator<pcl::PointCloud<PointT> >;

    boost::filesystem::path sweep_path;

    iterator begin()
    {
        return iterator(std::vector<std::string>({sweep_path.string()}), "convex_segments", "segment");
    }

    iterator end()
    {
        return iterator();
    }

    sweep_convex_segment_cloud_map(const boost::filesystem::path& sweep_path) : sweep_path(sweep_path / "room.xml") {}
};

struct sweep_convex_feature_cloud_map {

    using iterator = segment_cloud_iterator<pcl::PointCloud<HistT> >;

    boost::filesystem::path sweep_path;

    iterator begin()
    {
        return iterator(std::vector<std::string>({sweep_path.string()}), "convex_segments", "pfhrgbfeature");
    }

    iterator end()
    {
        return iterator();
    }

    sweep_convex_feature_cloud_map(const boost::filesystem::path& sweep_path) : sweep_path(sweep_path / "room.xml") {}
};

struct sweep_convex_keypoint_cloud_map {

    using iterator = segment_cloud_iterator<pcl::PointCloud<PointT> >;

    boost::filesystem::path sweep_path;

    iterator begin()
    {
        return iterator(std::vector<std::string>({sweep_path.string()}), "convex_segments", "pfhrgbkeypoint");
    }

    iterator end()
    {
        return iterator();
    }

    sweep_convex_keypoint_cloud_map(const boost::filesystem::path& sweep_path) : sweep_path(sweep_path / "room.xml") {}
};

struct sweep_convex_segment_index_map {

    boost::filesystem::path sweep_path;

    sweep_segment_index_iterator begin()
    {
        return sweep_segment_index_iterator(std::vector<std::string>({sweep_path.string()}), "convex_segments", "segment");
    }

    sweep_segment_index_iterator end()
    {
        return sweep_segment_index_iterator();
    }

    sweep_convex_segment_index_map(const boost::filesystem::path& sweep_path) : sweep_path(sweep_path / "room.xml") {}
};

// iterators over subsegments of one sweep

struct sweep_subsegment_map {

    boost::filesystem::path sweep_path;

    segment_path_iterator begin()
    {
        return segment_path_iterator(std::vector<std::string>({sweep_path.string()}), "subsegments", "segment");
    }

    segment_path_iterator end()
    {
        return segment_path_iterator();
    }

    sweep_subsegment_map(const boost::filesystem::path& sweep_path) : sweep_path(sweep_path / "room.xml") {}
};

struct sweep_subsegment_cloud_map {

    using iterator = segment_cloud_iterator<pcl::PointCloud<PointT> >;

    boost::filesystem::path sweep_path;

    iterator begin()
    {
        return iterator(std::vector<std::string>({sweep_path.string()}), "subsegments", "segment");
    }

    iterator end()
    {
        return iterator();
    }

    sweep_subsegment_cloud_map(const boost::filesystem::path& sweep_path) : sweep_path(sweep_path / "room.xml") {}
};

struct sweep_subsegment_feature_map {

    boost::filesystem::path sweep_path;

    segment_path_iterator begin()
    {
        return segment_path_iterator(std::vector<std::string>({sweep_path.string()}), "subsegments", "pfhrgbfeature");
    }

    segment_path_iterator end()
    {
        return segment_path_iterator();
    }

    sweep_subsegment_feature_map(const boost::filesystem::path& sweep_path) : sweep_path(sweep_path / "room.xml") {}
};

struct sweep_subsegment_keypoint_map {

    boost::filesystem::path sweep_path;

    segment_path_iterator begin()
    {
        return segment_path_iterator(std::vector<std::string>({sweep_path.string()}), "subsegments", "pfhrgbkeypoint");
    }

    segment_path_iterator end()
    {
        return segment_path_iterator();
    }

    sweep_subsegment_keypoint_map(const boost::filesystem::path& sweep_path) : sweep_path(sweep_path / "room.xml") {}
};

struct sweep_subsegment_feature_cloud_map {

    using iterator = segment_cloud_iterator<pcl::PointCloud<HistT> >;

    boost::filesystem::path sweep_path;

    iterator begin()
    {
        return iterator(std::vector<std::string>({sweep_path.string()}), "subsegments", "pfhrgbfeature");
    }

    iterator end()
    {
        return iterator();
    }

    sweep_subsegment_feature_cloud_map(const boost::filesystem::path& sweep_path) : sweep_path(sweep_path / "room.xml") {}
};

// very convenient to have zipped iterators together with this
template <typename... T>
auto zip(T&... containers) -> boost::iterator_range<boost::zip_iterator<decltype(boost::make_tuple(containers.begin()...))>>
{
    auto zip_begin = boost::make_zip_iterator(boost::make_tuple(containers.begin()...));
    auto zip_end = boost::make_zip_iterator(boost::make_tuple(containers.end()...));
    return boost::make_iterator_range(zip_begin, zip_end);
}

// we could easily define iterators for segments of one room but it I can't see any use case

} // dynamic_object_retrieval

#endif // SUMMARY_ITERATORS_H
