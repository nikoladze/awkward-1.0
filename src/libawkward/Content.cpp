// BSD 3-Clause License; see https://github.com/scikit-hep/awkward-1.0/blob/master/LICENSE

#include <sstream>

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"

#include "awkward/cpu-kernels/operations.h"
#include "awkward/cpu-kernels/reducers.h"
#include "awkward/array/RegularArray.h"
#include "awkward/array/ListArray.h"
#include "awkward/array/ListOffsetArray.h"
#include "awkward/array/EmptyArray.h"
#include "awkward/array/UnionArray.h"
#include "awkward/array/IndexedArray.h"
#include "awkward/array/RecordArray.h"
#include "awkward/array/NumpyArray.h"
#include "awkward/array/ByteMaskedArray.h"
#include "awkward/array/BitMaskedArray.h"
#include "awkward/array/UnmaskedArray.h"
#include "awkward/array/VirtualArray.h"
#include "awkward/type/ArrayType.h"

#include "awkward/Content.h"

namespace rj = rapidjson;

namespace awkward {
  ////////// Form

  template <typename JSON>
  FormPtr
  fromjson_part(const JSON& json) {
    if (json.IsString()) {
      util::Parameters p;
      std::vector<int64_t> s;

      if (std::string("float64") == json.GetString()) {
        return std::make_shared<NumpyForm>(false, p, s, 8, "d");
      }
      if (std::string("float32") == json.GetString()) {
        return std::make_shared<NumpyForm>(false, p, s, 4, "f");
      }
      if (std::string("int64") == json.GetString()) {
#if defined _MSC_VER || defined __i386__
        return std::make_shared<NumpyForm>(false, p, s, 8, "q");
#else
        return std::make_shared<NumpyForm>(false, p, s, 8, "l");
#endif
      }
      if (std::string("uint64") == json.GetString()) {
#if defined _MSC_VER || defined __i386__
        return std::make_shared<NumpyForm>(false, p, s, 8, "Q");
#else
        return std::make_shared<NumpyForm>(false, p, s, 8, "L");
#endif
      }
      if (std::string("int32") == json.GetString()) {
#if defined _MSC_VER || defined __i386__
        return std::make_shared<NumpyForm>(false, p, s, 4, "l");
#else
        return std::make_shared<NumpyForm>(false, p, s, 4, "i");
#endif
      }
      if (std::string("uint32") == json.GetString()) {
#if defined _MSC_VER || defined __i386__
        return std::make_shared<NumpyForm>(false, p, s, 4, "L");
#else
        return std::make_shared<NumpyForm>(false, p, s, 4, "I");
#endif
      }
      if (std::string("int16") == json.GetString()) {
        return std::make_shared<NumpyForm>(false, p, s, 2, "h");
      }
      if (std::string("uint16") == json.GetString()) {
        return std::make_shared<NumpyForm>(false, p, s, 2, "H");
      }
      if (std::string("int8") == json.GetString()) {
        return std::make_shared<NumpyForm>(false, p, s, 1, "b");
      }
      if (std::string("uint8") == json.GetString()) {
        return std::make_shared<NumpyForm>(false, p, s, 1, "B");
      }
      if (std::string("bool") == json.GetString()) {
        return std::make_shared<NumpyForm>(false, p, s, 1, "?");
      }
    }

    if (json.IsObject()  &&
        json.HasMember("class")  &&
        json["class"].IsString()) {
      util::Parameters p;
      if (json.HasMember("parameters")) {
        if (json["parameters"].IsObject()) {
          for (auto& pair : json["parameters"].GetObject()) {
            rj::StringBuffer stringbuffer;
            rj::Writer<rj::StringBuffer> writer(stringbuffer);
            pair.value.Accept(writer);
            p[pair.name.GetString()] = stringbuffer.GetString();
          }
        }
        else {
          throw std::invalid_argument("'parameters' must be a JSON object");
        }
      }
      bool h = false;
      if (json.HasMember("has_identities")) {
        if (json["has_identities"].IsBool()) {
          h = json["has_identities"].GetBool();
        }
        else {
          throw std::invalid_argument("'has_identities' must be boolean");
        }
      }

      bool isgen;
      bool is64;
      bool isU32;
      bool is32;
      std::string cls = json["class"].GetString();

      if (cls == std::string("NumpyArray")) {
        std::string format;
        int64_t itemsize;
        if (json.HasMember("primitive")  &&  json["primitive"].IsString()) {
          FormPtr tmp = fromjson_part(json["primitive"]);
          NumpyForm* raw = dynamic_cast<NumpyForm*>(tmp.get());
          format = raw->format();
          itemsize = raw->itemsize();
        }
        else if (json.HasMember("format")  &&  json["format"].IsString()  &&
                 json.HasMember("itemsize")  &&  json["itemsize"].IsInt()) {
          format = json["format"].GetString();
          itemsize = json["itemsize"].GetInt64();
        }
        else {
          throw std::invalid_argument("NumpyForm must have a 'primitive' "
                                      "field or 'format' and 'itemsize'");
        }
        std::vector<int64_t> s;
        if (json.HasMember("inner_shape")  &&  json["inner_shape"].IsArray()) {
          for (auto& x : json["inner_shape"].GetArray()) {
            if (x.IsInt()) {
              s.push_back(x.GetInt64());
            }
            else {
              throw std::invalid_argument("NumpyForm 'inner_shape' must only "
                                          "contain integers");
            }
          }
        }
        return std::make_shared<NumpyForm>(h, p, s, itemsize, format);
      }

      if (cls == std::string("RecordArray")) {
        util::RecordLookupPtr recordlookup(nullptr);
        std::vector<FormPtr> contents;
        if (json.HasMember("contents")  &&  json["contents"].IsArray()) {
          for (auto& x : json["contents"].GetArray()) {
            contents.push_back(fromjson_part(x));
          }
        }
        else if (json.HasMember("contents")  &&  json["contents"].IsObject()) {
          recordlookup = std::make_shared<util::RecordLookup>();
          for (auto& pair : json["contents"].GetObject()) {
            recordlookup.get()->push_back(pair.name.GetString());
            contents.push_back(fromjson_part(pair.value));
          }
        }
        else {
          throw std::invalid_argument("RecordArray 'contents' must be a JSON "
                                      "list or a JSON object");
        }
        return std::make_shared<RecordForm>(h, p, recordlookup, contents);
      }

      if ((isgen = (cls == std::string("ListOffsetArray")))  ||
          (is64  = (cls == std::string("ListOffsetArray64")))  ||
          (isU32 = (cls == std::string("ListOffsetArrayU32")))  ||
          (is32  = (cls == std::string("ListOffsetArray32")))) {
        Index::Form offsets = (is64  ? Index::Form::i64 :
                               isU32 ? Index::Form::u32 :
                               is32  ? Index::Form::i32 :
                                       Index::Form::kNumIndexForm);
        if (json.HasMember("offsets")  &&  json["offsets"].IsString()) {
          Index::Form tmp = Index::str2form(json["offsets"].GetString());
          if (offsets != Index::Form::kNumIndexForm  &&  offsets != tmp) {
            throw std::invalid_argument(
                  cls + std::string(" has conflicting 'offsets' type: ")
                      + json["offsets"].GetString());
          }
          offsets = tmp;
        }
        if (offsets == Index::Form::kNumIndexForm) {
          throw std::invalid_argument(
                  cls + std::string(" is missing an 'offsets' specification"));
        }
        if (!json.HasMember("content")) {
          throw std::invalid_argument(
                  cls + std::string(" is missing its 'content'"));
        }
        FormPtr content = fromjson_part(json["content"]);
        return std::make_shared<ListOffsetForm>(h, p, offsets, content);
      }

      if ((isgen = (cls == std::string("ListArray")))  ||
          (is64  = (cls == std::string("ListArray64")))  ||
          (isU32 = (cls == std::string("ListArrayU32")))  ||
          (is32  = (cls == std::string("ListArray32")))) {
        Index::Form starts = (is64  ? Index::Form::i64 :
                              isU32 ? Index::Form::u32 :
                              is32  ? Index::Form::i32 :
                                      Index::Form::kNumIndexForm);
        Index::Form stops  = (is64  ? Index::Form::i64 :
                              isU32 ? Index::Form::u32 :
                              is32  ? Index::Form::i32 :
                                      Index::Form::kNumIndexForm);
        if (json.HasMember("starts")  &&  json["starts"].IsString()) {
          Index::Form tmp = Index::str2form(json["starts"].GetString());
          if (starts != Index::Form::kNumIndexForm  &&  starts != tmp) {
            throw std::invalid_argument(
                  cls + std::string(" has conflicting 'starts' type: ")
                      + json["starts"].GetString());
          }
          starts = tmp;
        }
        if (json.HasMember("stops")  &&  json["stops"].IsString()) {
          Index::Form tmp = Index::str2form(json["stops"].GetString());
          if (stops != Index::Form::kNumIndexForm  &&  stops != tmp) {
            throw std::invalid_argument(
                  cls + std::string(" has conflicting 'stops' type: ")
                      + json["stops"].GetString());
          }
          stops = tmp;
        }
        if (starts == Index::Form::kNumIndexForm) {
          throw std::invalid_argument(
                  cls + std::string(" is missing a 'starts' specification"));
        }
        if (stops == Index::Form::kNumIndexForm) {
          throw std::invalid_argument(
                  cls + std::string(" is missing a 'stops' specification"));
        }
        if (!json.HasMember("content")) {
          throw std::invalid_argument(
                  cls + std::string(" is missing its 'content'"));
        }
        FormPtr content = fromjson_part(json["content"]);
        return std::make_shared<ListForm>(h, p, starts, stops, content);
      }

      if (cls == std::string("RegularArray")) {
        if (!json.HasMember("content")) {
          throw std::invalid_argument(
                  cls + std::string(" is missing its 'content'"));
        }
        FormPtr content = fromjson_part(json["content"]);
        if (!json.HasMember("size")  ||  !json["size"].IsInt()) {
          throw std::invalid_argument(
                  cls + std::string(" is missing its 'size'"));
        }
        int64_t size = json["size"].GetInt64();
        return std::make_shared<RegularForm>(h, p, content, size);
      }

      if ((isgen = (cls == std::string("IndexedOptionArray")))  ||
          (is64  = (cls == std::string("IndexedOptionArray64")))  ||
          (is32  = (cls == std::string("IndexedOptionArray32")))) {
        Index::Form index = (is64  ? Index::Form::i64 :
                             is32  ? Index::Form::i32 :
                                     Index::Form::kNumIndexForm);
        if (json.HasMember("index")  &&  json["index"].IsString()) {
          Index::Form tmp = Index::str2form(json["index"].GetString());
          if (index != Index::Form::kNumIndexForm  &&  index != tmp) {
            throw std::invalid_argument(
                  cls + std::string(" has conflicting 'index' type: ")
                      + json["index"].GetString());
          }
          index = tmp;
        }
        if (index == Index::Form::kNumIndexForm) {
          throw std::invalid_argument(
                  cls + std::string(" is missing an 'index' specification"));
        }
        if (!json.HasMember("content")) {
          throw std::invalid_argument(
                  cls + std::string(" is missing its 'content'"));
        }
        FormPtr content = fromjson_part(json["content"]);
        return std::make_shared<IndexedOptionForm>(h, p, index, content);
      }

      if ((isgen = (cls == std::string("IndexedArray")))  ||
          (is64  = (cls == std::string("IndexedArray64")))  ||
          (isU32 = (cls == std::string("IndexedArrayU32")))  ||
          (is32  = (cls == std::string("IndexedArray32")))) {
        Index::Form index = (is64  ? Index::Form::i64 :
                             isU32 ? Index::Form::u32 :
                             is32  ? Index::Form::i32 :
                                     Index::Form::kNumIndexForm);
        if (json.HasMember("index")  &&  json["index"].IsString()) {
          Index::Form tmp = Index::str2form(json["index"].GetString());
          if (index != Index::Form::kNumIndexForm  &&  index != tmp) {
            throw std::invalid_argument(
                  cls + std::string(" has conflicting 'index' type: ")
                      + json["index"].GetString());
          }
          index = tmp;
        }
        if (index == Index::Form::kNumIndexForm) {
          throw std::invalid_argument(
                  cls + std::string(" is missing an 'index' specification"));
        }
        if (!json.HasMember("content")) {
          throw std::invalid_argument(
                  cls + std::string(" is missing its 'content'"));
        }
        FormPtr content = fromjson_part(json["content"]);
        return std::make_shared<IndexedForm>(h, p, index, content);
      }

      if (cls == std::string("ByteMaskedArray")) {
        Index::Form mask = (is64  ? Index::Form::i64 :
                            isU32 ? Index::Form::u32 :
                            is32  ? Index::Form::i32 :
                                    Index::Form::kNumIndexForm);
        if (json.HasMember("mask")  &&  json["mask"].IsString()) {
          Index::Form tmp = Index::str2form(json["mask"].GetString());
          if (mask != Index::Form::kNumIndexForm  &&  mask != tmp) {
            throw std::invalid_argument(
                  cls + std::string(" has conflicting 'mask' type: ")
                      + json["mask"].GetString());
          }
          mask = tmp;
        }
        if (mask == Index::Form::kNumIndexForm) {
          throw std::invalid_argument(
                  cls + std::string(" is missing a 'mask' specification"));
        }
        if (!json.HasMember("content")) {
          throw std::invalid_argument(
                  cls + std::string(" is missing its 'content'"));
        }
        FormPtr content = fromjson_part(json["content"]);
        if (!json.HasMember("valid_when")  ||  !json["valid_when"].IsBool()) {
          throw std::invalid_argument(
                  cls + std::string(" is missing its 'valid_when'"));
        }
        bool valid_when = json["valid_when"].GetBool();
        return std::make_shared<ByteMaskedForm>(h, p, mask, content,
                                                valid_when);
      }

      if (cls == std::string("BitMaskedArray")) {
        Index::Form mask = (is64  ? Index::Form::i64 :
                            isU32 ? Index::Form::u32 :
                            is32  ? Index::Form::i32 :
                                    Index::Form::kNumIndexForm);
        if (json.HasMember("mask")  &&  json["mask"].IsString()) {
          Index::Form tmp = Index::str2form(json["mask"].GetString());
          if (mask != Index::Form::kNumIndexForm  &&  mask != tmp) {
            throw std::invalid_argument(
                  cls + std::string(" has conflicting 'mask' type: ")
                      + json["mask"].GetString());
          }
          mask = tmp;
        }
        if (mask == Index::Form::kNumIndexForm) {
          throw std::invalid_argument(
                  cls + std::string(" is missing a 'mask' specification"));
        }
        if (!json.HasMember("content")) {
          throw std::invalid_argument(
                  cls + std::string(" is missing its 'content'"));
        }
        FormPtr content = fromjson_part(json["content"]);
        if (!json.HasMember("valid_when")  ||  !json["valid_when"].IsBool()) {
          throw std::invalid_argument(
                  cls + std::string(" is missing its 'valid_when'"));
        }
        bool valid_when = json["valid_when"].GetBool();
        if (!json.HasMember("lsb_order")  ||  !json["lsb_order"].IsBool()) {
          throw std::invalid_argument(
                  cls + std::string(" is missing its 'lsb_order'"));
        }
        bool lsb_order = json["lsb_order"].GetBool();
        return std::make_shared<BitMaskedForm>(h, p, mask, content,
                                               valid_when, lsb_order);
      }

      if (cls == std::string("UnmaskedArray")) {
        if (!json.HasMember("content")) {
          throw std::invalid_argument(
                  cls + std::string(" is missing its 'content'"));
        }
        FormPtr content = fromjson_part(json["content"]);
        return std::make_shared<UnmaskedForm>(h, p, content);
      }

      if ((isgen = (cls == std::string("UnionArray")))  ||
          (is64  = (cls == std::string("UnionArray8_64")))  ||
          (isU32 = (cls == std::string("UnionArray8_U32")))  ||
          (is32  = (cls == std::string("UnionArray8_32")))) {
        Index::Form tags = (is64  ? Index::Form::i8 :
                            isU32 ? Index::Form::i8 :
                            is32  ? Index::Form::i8 :
                                    Index::Form::kNumIndexForm);
        if (json.HasMember("tags")  &&  json["tags"].IsString()) {
          Index::Form tmp = Index::str2form(json["tags"].GetString());
          if (tags != Index::Form::kNumIndexForm  &&  tags != tmp) {
            throw std::invalid_argument(
                  cls + std::string(" has conflicting 'tags' type: ")
                      + json["tags"].GetString());
          }
          tags = tmp;
        }
        Index::Form index = (is64  ? Index::Form::i64 :
                             isU32 ? Index::Form::u32 :
                             is32  ? Index::Form::i32 :
                                     Index::Form::kNumIndexForm);
        if (json.HasMember("index")  &&  json["index"].IsString()) {
          Index::Form tmp = Index::str2form(json["index"].GetString());
          if (index != Index::Form::kNumIndexForm  &&  index != tmp) {
            throw std::invalid_argument(
                  cls + std::string(" has conflicting 'index' type: ")
                      + json["index"].GetString());
          }
          index = tmp;
        }
        if (tags == Index::Form::kNumIndexForm) {
          throw std::invalid_argument(
                  cls + std::string(" is missing a 'tags' specification"));
        }
        if (index == Index::Form::kNumIndexForm) {
          throw std::invalid_argument(
                  cls + std::string(" is missing an 'index' specification"));
        }
        std::vector<FormPtr> contents;
        if (json.HasMember("contents")  &&  json["contents"].IsArray()) {
          for (auto& x : json["contents"].GetArray()) {
            contents.push_back(fromjson_part(x));
          }
        }
        else {
          throw std::invalid_argument(
                  cls + std::string(" 'contents' must be a JSON list"));
        }
        return std::make_shared<UnionForm>(h, p, tags, index, contents);
      }

      if (cls == std::string("EmptyArray")) {
        return std::make_shared<EmptyForm>(h, p);
      }

      if (cls == std::string("VirtualArray")) {
        if (!json.HasMember("form")) {
          throw std::invalid_argument(
                  cls + std::string(" is missing its 'form'"));
        }
        FormPtr form(nullptr);
        if (!json["form"].IsNull()) {
          form = fromjson_part(json["form"]);
        }
        if (!json.HasMember("has_length")  ||  !json["has_length"].IsBool()) {
          throw std::invalid_argument(
                  cls + std::string(" is missing its 'has_length'"));
        }
        bool has_length = json["has_length"].GetBool();
        return std::make_shared<VirtualForm>(h, p, form, has_length);
      }

    }

    rj::StringBuffer stringbuffer;
    rj::PrettyWriter<rj::StringBuffer> writer(stringbuffer);
    json.Accept(writer);
    throw std::invalid_argument(
              std::string("JSON cannot be recognized as a Form:\n\n")
              + stringbuffer.GetString());
  }

  FormPtr
  Form::fromjson(const std::string& data) {
    rj::Document doc;
    doc.Parse<rj::kParseNanAndInfFlag>(data.c_str());
    return fromjson_part(doc);
  }

  Form::Form(bool has_identities, const util::Parameters& parameters)
      : has_identities_(has_identities)
      , parameters_(parameters) { }

  const std::string
  Form::tostring() const {
    return tojson(true, false);
  }

  const std::string
  Form::tojson(bool pretty, bool verbose) const {
    if (pretty) {
      ToJsonPrettyString builder(-1);
      tojson_part(builder, verbose);
      return builder.tostring();
    }
    else {
      ToJsonString builder(-1);
      tojson_part(builder, verbose);
      return builder.tostring();
    }
  }

  bool
  Form::has_identities() const {
    return has_identities_;
  }

  const util::Parameters
  Form::parameters() const {
    return parameters_;
  }

  const std::string
  Form::parameter(const std::string& key) const {
    auto item = parameters_.find(key);
    if (item == parameters_.end()) {
      return "null";
    }
    return item->second;
  }

  bool
  Form::parameter_equals(const std::string& key,
                         const std::string& value) const {
    return util::parameter_equals(parameters_, key, value);
  }

  void
  Form::identities_tojson(ToJson& builder, bool verbose) const {
    if (verbose  ||  has_identities_) {
      builder.field("has_identities");
      builder.boolean(has_identities_);
    }
  }

  void
  Form::parameters_tojson(ToJson& builder, bool verbose) const {
    if (verbose  ||  !parameters_.empty()) {
      builder.field("parameters");
      builder.beginrecord();
      for (auto pair : parameters_) {
        builder.field(pair.first.c_str());
        builder.json(pair.second.c_str());
      }
      builder.endrecord();
    }
  }

  ////////// Content

  Content::Content(const IdentitiesPtr& identities,
                   const util::Parameters& parameters)
      : identities_(identities)
      , parameters_(parameters) { }

  bool
  Content::isscalar() const {
    return false;
  }

  const IdentitiesPtr
  Content::identities() const {
    return identities_;
  }

  const std::string
  Content::tostring() const {
    return tostring_part("", "", "");
  }

  const std::string
  Content::tojson(bool pretty, int64_t maxdecimals) const {
    if (pretty) {
      ToJsonPrettyString builder(maxdecimals);
      tojson_part(builder, true);
      return builder.tostring();
    }
    else {
      ToJsonString builder(maxdecimals);
      tojson_part(builder, true);
      return builder.tostring();
    }
  }

  void
  Content::tojson(FILE* destination,
                  bool pretty,
                  int64_t maxdecimals,
                  int64_t buffersize) const {
    if (pretty) {
      ToJsonPrettyFile builder(destination, maxdecimals, buffersize);
      builder.beginlist();
      tojson_part(builder, true);
      builder.endlist();
    }
    else {
      ToJsonFile builder(destination, maxdecimals, buffersize);
      builder.beginlist();
      tojson_part(builder, true);
      builder.endlist();
    }
  }

  int64_t
  Content::nbytes() const {
    // FIXME: this is only accurate if all subintervals of allocated arrays are
    // nested (which is likely, but not guaranteed). In general, it's <= the
    // correct nbytes.
    std::map<size_t, int64_t> largest;
    nbytes_part(largest);
    int64_t out = 0;
    for (auto pair : largest) {
      out += pair.second;
    }
    return out;
  }

  const std::string
  Content::purelist_parameter(const std::string& key) const {
    return form(false).get()->purelist_parameter(key);
  }

  bool
  Content::purelist_isregular() const {
    return form(true).get()->purelist_isregular();
  }

  int64_t
  Content::purelist_depth() const {
    return form(true).get()->purelist_depth();
  }

  const std::pair<int64_t, int64_t>
  Content::minmax_depth() const {
    return form(true).get()->minmax_depth();
  }

  const std::pair<bool, int64_t>
  Content::branch_depth() const {
    return form(true).get()->branch_depth();
  }

  const ContentPtr
  Content::reduce(const Reducer& reducer,
                  int64_t axis,
                  bool mask,
                  bool keepdims) const {
    int64_t negaxis = -axis;
    std::pair<bool, int64_t> branchdepth = branch_depth();
    bool branch = branchdepth.first;
    int64_t depth = branchdepth.second;

    if (branch) {
      if (negaxis <= 0) {
        throw std::invalid_argument(
        "cannot use non-negative axis on a nested list structure "
        "of variable depth (negative axis counts from the leaves of the tree; "
        "non-negative from the root)");
      }
      if (negaxis > depth) {
        throw std::invalid_argument(
          std::string("cannot use axis=") + std::to_string(axis)
          + std::string(" on a nested list structure that splits into "
                        "different depths, the minimum of which is depth=")
          + std::to_string(depth) + std::string(" from the leaves"));
      }
    }
    else {
      if (negaxis <= 0) {
        negaxis += depth;
      }
      if (!(0 < negaxis  &&  negaxis <= depth)) {
        throw std::invalid_argument(
          std::string("axis=") + std::to_string(axis)
          + std::string(" exceeds the depth of the nested list structure "
                        "(which is ")
          + std::to_string(depth) + std::string(")"));
      }
    }

    Index64 starts(1);
    starts.setitem_at_nowrap(0, 0);

    Index64 parents(length());
    struct Error err = awkward_content_reduce_zeroparents_64(
      parents.ptr().get(),
      length());
    util::handle_error(err, classname(), identities_.get());

    ContentPtr next = reduce_next(reducer,
                                  negaxis,
                                  starts,
                                  parents,
                                  1,
                                  mask,
                                  keepdims);
    return next.get()->getitem_at_nowrap(0);
  }

  const util::Parameters
  Content::parameters() const {
    return parameters_;
  }

  void
  Content::setparameters(const util::Parameters& parameters) {
    parameters_ = parameters;
  }

  const std::string
  Content::parameter(const std::string& key) const {
    auto item = parameters_.find(key);
    if (item == parameters_.end()) {
      return "null";
    }
    return item->second;
  }

  void
  Content::setparameter(const std::string& key, const std::string& value) {
    if (value == std::string("null")) {
      parameters_.erase(key);
    }
    else {
      parameters_[key] = value;
    }
  }

  bool
  Content::parameter_equals(const std::string& key,
                            const std::string& value) const {
    return util::parameter_equals(parameters_, key, value);
  }

  bool
  Content::parameters_equal(const util::Parameters& other) const {
    return util::parameters_equal(parameters_, other);
  }

  bool
  Content::parameter_isstring(const std::string& key) const {
    return util::parameter_isstring(parameters_, key);
  }

  bool
  Content::parameter_isname(const std::string& key) const {
    return util::parameter_isname(parameters_, key);
  }

  const std::string
  Content::parameter_asstring(const std::string& key) const {
    return util::parameter_asstring(parameters_, key);
  }

  const ContentPtr
  Content::merge_as_union(const ContentPtr& other) const {
    int64_t mylength = length();
    int64_t theirlength = other.get()->length();
    Index8 tags(mylength + theirlength);
    Index64 index(mylength + theirlength);

    ContentPtrVec contents({ shallow_copy(), other });

    struct Error err1 = awkward_unionarray_filltags_to8_const(
      tags.ptr().get(),
      0,
      mylength,
      0);
    util::handle_error(err1, classname(), identities_.get());
    struct Error err2 = awkward_unionarray_fillindex_to64_count(
      index.ptr().get(),
      0,
      mylength);
    util::handle_error(err2, classname(), identities_.get());

    struct Error err3 = awkward_unionarray_filltags_to8_const(
      tags.ptr().get(),
      mylength,
      theirlength,
      1);
    util::handle_error(err3, classname(), identities_.get());
    struct Error err4 = awkward_unionarray_fillindex_to64_count(
      index.ptr().get(),
      mylength,
      theirlength);
    util::handle_error(err4, classname(), identities_.get());

    return std::make_shared<UnionArray8_64>(Identities::none(),
                                            util::Parameters(),
                                            tags,
                                            index,
                                            contents);
  }

  const ContentPtr
  Content::rpad_axis0(int64_t target, bool clip) const {
    if (!clip  &&  target < length()) {
      return shallow_copy();
    }
    Index64 index(target);
    struct Error err = awkward_index_rpad_and_clip_axis0_64(
      index.ptr().get(),
      target,
      length());
    util::handle_error(err, classname(), identities_.get());
    std::shared_ptr<IndexedOptionArray64> next =
      std::make_shared<IndexedOptionArray64>(Identities::none(),
                                             util::Parameters(),
                                             index,
                                             shallow_copy());
    return next.get()->simplify_optiontype();
  }

  const ContentPtr
  Content::localindex_axis0() const {
    Index64 localindex(length());
    struct Error err = awkward_localindex_64(
      localindex.ptr().get(),
      length());
    util::handle_error(err, classname(), identities_.get());
    return std::make_shared<NumpyArray>(localindex);
  }

  const ContentPtr
  Content::combinations_axis0(int64_t n,
                              bool replacement,
                              const util::RecordLookupPtr& recordlookup,
                              const util::Parameters& parameters) const {
    int64_t size = length();
    if (replacement) {
      size += (n - 1);
    }
    int64_t thisn = n;
    int64_t combinationslen;
    if (thisn > size) {
      combinationslen = 0;
    }
    else if (thisn == size) {
      combinationslen = 1;
    }
    else {
      if (thisn * 2 > size) {
        thisn = size - thisn;
      }
      combinationslen = size;
      for (int64_t j = 2;  j <= thisn;  j++) {
        combinationslen *= (size - j + 1);
        combinationslen /= j;
      }
    }

    std::vector<std::shared_ptr<int64_t>> tocarry;
    std::vector<int64_t*> tocarryraw;
    for (int64_t j = 0;  j < n;  j++) {
      std::shared_ptr<int64_t> ptr(new int64_t[(size_t)combinationslen],
                                   util::array_deleter<int64_t>());
      tocarry.push_back(ptr);
      tocarryraw.push_back(ptr.get());
    }
    struct Error err = awkward_regulararray_combinations_64(
      tocarryraw.data(),
      n,
      replacement,
      length(),
      1);
    util::handle_error(err, classname(), identities_.get());

    ContentPtrVec contents;
    for (auto ptr : tocarry) {
      contents.push_back(std::make_shared<IndexedArray64>(
        Identities::none(),
        util::Parameters(),
        Index64(ptr, 0, combinationslen),
        shallow_copy()));
    }
    return std::make_shared<RecordArray>(Identities::none(),
                                         parameters,
                                         contents,
                                         recordlookup);
  }

  const ContentPtr
  Content::getitem(const Slice& where) const {
    ContentPtr next = std::make_shared<RegularArray>(Identities::none(),
                                                     util::Parameters(),
                                                     shallow_copy(),
                                                     length());
    SliceItemPtr nexthead = where.head();
    Slice nexttail = where.tail();
    Index64 nextadvanced(0);
    ContentPtr out = next.get()->getitem_next(nexthead,
                                              nexttail,
                                              nextadvanced);

    if (out.get()->length() == 0) {
      return out.get()->getitem_nothing();
    }
    else {
      return out.get()->getitem_at_nowrap(0);
    }
  }

  const ContentPtr
  Content::getitem_next(const SliceItemPtr& head,
                        const Slice& tail,
                        const Index64& advanced) const {
    if (head.get() == nullptr) {
      return shallow_copy();
    }
    else if (SliceAt* at =
             dynamic_cast<SliceAt*>(head.get())) {
      return getitem_next(*at, tail, advanced);
    }
    else if (SliceRange* range =
             dynamic_cast<SliceRange*>(head.get())) {
      return getitem_next(*range, tail, advanced);
    }
    else if (SliceEllipsis* ellipsis =
             dynamic_cast<SliceEllipsis*>(head.get())) {
      return getitem_next(*ellipsis, tail, advanced);
    }
    else if (SliceNewAxis* newaxis =
             dynamic_cast<SliceNewAxis*>(head.get())) {
      return getitem_next(*newaxis, tail, advanced);
    }
    else if (SliceArray64* array =
             dynamic_cast<SliceArray64*>(head.get())) {
      return getitem_next(*array, tail, advanced);
    }
    else if (SliceField* field =
             dynamic_cast<SliceField*>(head.get())) {
      return getitem_next(*field, tail, advanced);
    }
    else if (SliceFields* fields =
             dynamic_cast<SliceFields*>(head.get())) {
      return getitem_next(*fields, tail, advanced);
    }
    else if (SliceMissing64* missing =
             dynamic_cast<SliceMissing64*>(head.get())) {
      return getitem_next(*missing, tail, advanced);
    }
    else if (SliceJagged64* jagged =
             dynamic_cast<SliceJagged64*>(head.get())) {
      return getitem_next(*jagged, tail, advanced);
    }
    else {
      throw std::runtime_error("unrecognized slice type");
    }
  }

  const ContentPtr
  Content::getitem_next_jagged(const Index64& slicestarts,
                               const Index64& slicestops,
                               const SliceItemPtr& slicecontent,
                               const Slice& tail) const {
    if (SliceArray64* array =
        dynamic_cast<SliceArray64*>(slicecontent.get())) {
      return getitem_next_jagged(slicestarts, slicestops, *array, tail);
    }
    else if (SliceMissing64* missing =
             dynamic_cast<SliceMissing64*>(slicecontent.get())) {
      return getitem_next_jagged(slicestarts, slicestops, *missing, tail);
    }
    else if (SliceJagged64* jagged =
             dynamic_cast<SliceJagged64*>(slicecontent.get())) {
      return getitem_next_jagged(slicestarts, slicestops, *jagged, tail);
    }
    else {
      throw std::runtime_error(
        "unexpected slice type for getitem_next_jagged");
    }
  }

  const ContentPtr
  Content::getitem_next(const SliceEllipsis& ellipsis,
                        const Slice& tail,
                        const Index64& advanced) const {
    std::pair<int64_t, int64_t> minmax = minmax_depth();
    int64_t mindepth = minmax.first;
    int64_t maxdepth = minmax.second;

    if (tail.length() == 0  ||
        (mindepth - 1 == tail.dimlength()  &&
         maxdepth - 1 == tail.dimlength())) {
      SliceItemPtr nexthead = tail.head();
      Slice nexttail = tail.tail();
      return getitem_next(nexthead, nexttail, advanced);
    }
    else if (mindepth - 1 == tail.dimlength()  ||
             maxdepth - 1 == tail.dimlength()) {
      throw std::invalid_argument(
        "ellipsis (...) can't be used on a data structure of "
        "different depths");
    }
    else {
      std::vector<SliceItemPtr> tailitems = tail.items();
      std::vector<SliceItemPtr> items = { std::make_shared<SliceEllipsis>() };
      items.insert(items.end(), tailitems.begin(), tailitems.end());
      SliceItemPtr nexthead = std::make_shared<SliceRange>(Slice::none(),
                                                           Slice::none(),
                                                           1);
      Slice nexttail(items);
      return getitem_next(nexthead, nexttail, advanced);
    }
  }

  const ContentPtr
  Content::getitem_next(const SliceNewAxis& newaxis,
                        const Slice& tail,
                        const Index64& advanced) const {
    SliceItemPtr nexthead = tail.head();
    Slice nexttail = tail.tail();
    return std::make_shared<RegularArray>(
      Identities::none(),
      util::Parameters(),
      getitem_next(nexthead, nexttail, advanced),
      1);
  }

  const ContentPtr
  Content::getitem_next(const SliceField& field,
                        const Slice& tail,
                        const Index64& advanced) const {
    SliceItemPtr nexthead = tail.head();
    Slice nexttail = tail.tail();
    return getitem_field(field.key()).get()->getitem_next(nexthead,
                                                          nexttail,
                                                          advanced);
  }

  const ContentPtr
  Content::getitem_next(const SliceFields& fields,
                        const Slice& tail,
                        const Index64& advanced) const {
    SliceItemPtr nexthead = tail.head();
    Slice nexttail = tail.tail();
    return getitem_fields(fields.keys()).get()->getitem_next(nexthead,
                                                             nexttail,
                                                             advanced);
  }

  const ContentPtr getitem_next_regular_missing(const SliceMissing64& missing,
                                                const Slice& tail,
                                                const Index64& advanced,
                                                const RegularArray* raw,
                                                int64_t length,
                                                const std::string& classname) {
    Index64 index(missing.index());
    Index64 outindex(index.length()*length);

    struct Error err = awkward_missing_repeat_64(
      outindex.ptr().get(),
      index.ptr().get(),
      index.offset(),
      index.length(),
      length,
      raw->size());
    util::handle_error(err, classname, nullptr);

    IndexedOptionArray64 out(Identities::none(),
                             util::Parameters(),
                             outindex,
                             raw->content());
    return std::make_shared<RegularArray>(Identities::none(),
                                          util::Parameters(),
                                          out.simplify_optiontype(),
                                          index.length());
  }

  bool check_missing_jagged_same(const ContentPtr& that,
                                 const Index8& bytemask,
                                 const SliceMissing64& missing) {
    if (bytemask.length() != missing.length()) {
      return false;
    }
    Index64 missingindex = missing.index();
    bool same;
    struct Error err = awkward_slicemissing_check_same(
      &same,
      bytemask.ptr().get(),
      bytemask.offset(),
      missingindex.ptr().get(),
      missingindex.offset(),
      bytemask.length());
    util::handle_error(err,
                       that.get()->classname(),
                       that.get()->identities().get());
    return same;
  }

  const ContentPtr check_missing_jagged(const ContentPtr& that,
                                        const SliceMissing64& missing) {
    // FIXME: This function is insufficiently general. While working on
    // something else, I noticed that it wasn't possible to slice option-type
    // data with a jagged array. This handles the case where that happens at
    // top-level; the most likely case for physics analysis, but it should be
    // more deeply considered in general.

    // Note that it only replaces the Content that would be passed to
    // getitem_next(missing.content()) in getitem_next(SliceMissing64) in a
    // particular scenario; it can probably be generalized by handling more
    // general scenarios.

    if (that.get()->length() == 1  &&
        dynamic_cast<SliceJagged64*>(missing.content().get())) {
      ContentPtr tmp1 = that.get()->getitem_at_nowrap(0);
      ContentPtr tmp2(nullptr);
      if (IndexedOptionArray32* rawtmp1 =
          dynamic_cast<IndexedOptionArray32*>(tmp1.get())) {
        tmp2 = rawtmp1->project();
        if (!check_missing_jagged_same(that, rawtmp1->bytemask(), missing)) {
          return that;
        }
      }
      else if (IndexedOptionArray64* rawtmp1 =
               dynamic_cast<IndexedOptionArray64*>(tmp1.get())) {
        tmp2 = rawtmp1->project();
        if (!check_missing_jagged_same(that, rawtmp1->bytemask(), missing)) {
          return that;
        }
      }
      else if (ByteMaskedArray* rawtmp1 =
               dynamic_cast<ByteMaskedArray*>(tmp1.get())) {
        tmp2 = rawtmp1->project();
        if (!check_missing_jagged_same(that, rawtmp1->bytemask(), missing)) {
          return that;
        }
      }
      else if (BitMaskedArray* rawtmp1 =
               dynamic_cast<BitMaskedArray*>(tmp1.get())) {
        tmp2 = rawtmp1->project();
        if (!check_missing_jagged_same(that, rawtmp1->bytemask(), missing)) {
          return that;
        }
      }

      if (tmp2.get() != nullptr) {
        return std::make_shared<RegularArray>(Identities::none(),
                                              that.get()->parameters(),
                                              tmp2,
                                              tmp2.get()->length());
      }
    }
    return that;
  }

  const ContentPtr
  Content::getitem_next(const SliceMissing64& missing,
                        const Slice& tail,
                        const Index64& advanced) const {
    if (advanced.length() != 0) {
      throw std::invalid_argument("cannot mix missing values in slice "
                                  "with NumPy-style advanced indexing");
    }

    ContentPtr tmp = check_missing_jagged(shallow_copy(), missing);
    ContentPtr next = tmp.get()->getitem_next(missing.content(),
                                              tail,
                                              advanced);

    if (RegularArray* raw = dynamic_cast<RegularArray*>(next.get())) {
      return getitem_next_regular_missing(missing,
                                          tail,
                                          advanced,
                                          raw,
                                          length(),
                                          classname());
    }

    else if (RecordArray* rec = dynamic_cast<RecordArray*>(next.get())) {
      if (rec->numfields() == 0) {
        return next;
      }
      ContentPtrVec contents;
      for (auto content : rec->contents()) {
        if (RegularArray* raw = dynamic_cast<RegularArray*>(content.get())) {
          contents.push_back(getitem_next_regular_missing(missing,
                                                          tail,
                                                          advanced,
                                                          raw,
                                                          length(),
                                                          classname()));
        }
        else {
          throw std::runtime_error(
            std::string("FIXME: unhandled case of SliceMissing with ")
            + std::string("RecordArray containing\n")
            + content.get()->tostring());
        }
      }
      return std::make_shared<RecordArray>(Identities::none(),
                                           util::Parameters(),
                                           contents,
                                           rec->recordlookup());
    }

    else {
      throw std::runtime_error(
        std::string("FIXME: unhandled case of SliceMissing with\n")
        + next.get()->tostring());
    }
  }

  int64_t
  Content::axis_wrap_if_negative(int64_t axis) {
    if (axis < 0) {
      throw std::runtime_error("FIXME: negative axis not implemented yet");
    }
    return axis;
  }

  const ContentPtr
  Content::getitem_next_array_wrap(const ContentPtr& outcontent,
                                   const std::vector<int64_t>& shape) const {
    ContentPtr out =
      std::make_shared<RegularArray>(Identities::none(),
                                     util::Parameters(),
                                     outcontent,
                                     (int64_t)shape[shape.size() - 1]);
    for (int64_t i = (int64_t)shape.size() - 2;  i >= 0;  i--) {
      out = std::make_shared<RegularArray>(Identities::none(),
                                           util::Parameters(),
                                           out,
                                           (int64_t)shape[(size_t)i]);
    }
    return out;
  }

  const std::string
  Content::parameters_tostring(const std::string& indent,
                               const std::string& pre,
                               const std::string& post) const {
    if (parameters_.empty()) {
      return "";
    }
    else {
      std::stringstream out;
      out << indent << pre << "<parameters>\n";
      for (auto pair : parameters_) {
        out << indent << "    <param key=" << util::quote(pair.first, true)
            << ">" << pair.second << "</param>\n";
      }
      out << indent << "</parameters>" << post;
      return out.str();
    }
  }
}
