/** 
 * @file llinitparam.h
 * @brief parameter block abstraction for creating complex objects and 
 * parsing construction parameters from xml and LLSD
 *
 * $LicenseInfo:firstyear=2008&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#ifndef LL_LLPARAM_H
#define LL_LLPARAM_H

#include <vector>
#include <boost/function.hpp>
#include <boost/type_traits/is_convertible.hpp>
#include <boost/unordered_map.hpp>
#include <boost/shared_ptr.hpp>

namespace LLInitParam
{
	template<typename T> const T& defaultValue() { static T value; return value; }

	template <typename T, bool IS_BOOST_FUNCTION = boost::is_convertible<T, boost::function_base>::value >
    struct ParamCompare 
	{
    	static bool equals(const T &a, const T &b)
		{
			return a == b;
		}
    };
    
	// boost function types are not comparable
	template<typename T>
	struct ParamCompare<T, true>
	{
		static bool equals(const T&a, const T &b)
		{
			return false;
		}
	};

	template<> 
	struct ParamCompare<LLSD, false>
	{
		static bool equals(const LLSD &a, const LLSD &b) { return false; }
	};

	// helper functions and classes
	typedef ptrdiff_t param_handle_t;

	// empty default implementation of key cache
	// leverages empty base class optimization
	template <typename T>
	class TypeValues
	{
	public:
		typedef std::map<std::string, T> value_name_map_t;

		void setValueName(const std::string& key) {}
		std::string getValueName() const { return ""; }
		void clearValueName() const {}

		static bool getValueFromName(const std::string& name, T& value)
		{
			return false;
		}

		static bool valueNamesExist()
		{
			return false;
		}

		static std::vector<std::string>* getPossibleValues()
		{
			return NULL;
		}

		static value_name_map_t* getValueNames() {return NULL;}
	};

	template <typename T, typename DERIVED_TYPE = TypeValues<T> >
	class TypeValuesHelper
	{
	public:
		typedef typename std::map<std::string, T> value_name_map_t;

		//TODO: cache key by index to save on param block size
		void setValueName(const std::string& value_name) 
		{
			mValueName = value_name; 
		}

		std::string getValueName() const
		{ 
			return mValueName; 
		}

		void clearValueName() const
		{
			mValueName.clear();
		}

		static bool getValueFromName(const std::string& name, T& value)
		{
			value_name_map_t* map = getValueNames();
			typename value_name_map_t::iterator found_it = map->find(name);
			if (found_it == map->end()) return false;

			value = found_it->second;
			return true;
		}

		static bool valueNamesExist()
		{
			return !getValueNames()->empty();
		}
	
		static value_name_map_t* getValueNames()
		{
			static value_name_map_t sMap;
			static bool sInitialized = false;

			if (!sInitialized)
			{
				sInitialized = true;
				DERIVED_TYPE::declareValues();
			}
			return &sMap;
		}

		static std::vector<std::string>* getPossibleValues()
		{
			static std::vector<std::string> sValues;

			value_name_map_t* map = getValueNames();
			for (typename value_name_map_t::iterator it = map->begin(), end_it = map->end();
				 it != end_it;
				 ++it)
			{
				sValues.push_back(it->first);
			}
			return &sValues;
		}

		static void declare(const std::string& name, const T& value)
		{
			(*getValueNames())[name] = value;
		}

	protected:
		static void getName(const std::string& name, const T& value)
		{}

		mutable std::string	mValueName;
	};

	class Parser
	{
		LOG_CLASS(Parser);

	public:
		
		struct CompareTypeID
		{
			bool operator()(const std::type_info* lhs, const std::type_info* rhs) const
			{
				return lhs->before(*rhs);
			}
		};

		typedef std::vector<std::pair<std::string, S32> >								name_stack_t;
		typedef std::pair<name_stack_t::const_iterator, name_stack_t::const_iterator>	name_stack_range_t;
		typedef std::vector<std::string>												possible_values_t;

		typedef bool (*parser_read_func_t)(Parser& parser, void* output);
		typedef bool (*parser_write_func_t)(Parser& parser, const void*, const name_stack_t&);
		typedef boost::function<void (const name_stack_t&, S32, S32, const possible_values_t*)>	parser_inspect_func_t;

		typedef std::map<const std::type_info*, parser_read_func_t, CompareTypeID>		parser_read_func_map_t;
		typedef std::map<const std::type_info*, parser_write_func_t, CompareTypeID>		parser_write_func_map_t;
		typedef std::map<const std::type_info*, parser_inspect_func_t, CompareTypeID>	parser_inspect_func_map_t;

		Parser(parser_read_func_map_t& read_map, parser_write_func_map_t& write_map, parser_inspect_func_map_t& inspect_map)
		:	mParseSilently(false),
			mParseGeneration(sNextParseGeneration),
			mParserReadFuncs(&read_map),
			mParserWriteFuncs(&write_map),
			mParserInspectFuncs(&inspect_map)
		{}
		virtual ~Parser();

		template <typename T> bool readValue(T& param)
	    {
		    parser_read_func_map_t::iterator found_it = mParserReadFuncs->find(&typeid(T));
		    if (found_it != mParserReadFuncs->end())
		    {
			    return found_it->second(*this, (void*)&param);
		    }
		    return false;
	    }

		template <typename T> bool writeValue(const T& param, const name_stack_t& name_stack)
		{
		    parser_write_func_map_t::iterator found_it = mParserWriteFuncs->find(&typeid(T));
		    if (found_it != mParserWriteFuncs->end())
		    {
			    return found_it->second(*this, (const void*)&param, name_stack);
		    }
		    return false;
		}

		// dispatch inspection to registered inspection functions, for each parameter in a param block
		template <typename T> bool inspectValue(const name_stack_t& name_stack, S32 min_count, S32 max_count, const possible_values_t* possible_values)
		{
		    parser_inspect_func_map_t::iterator found_it = mParserInspectFuncs->find(&typeid(T));
		    if (found_it != mParserInspectFuncs->end())
		    {
			    found_it->second(name_stack, min_count, max_count, possible_values);
				return true;
		    }
			return false;
		}

		virtual std::string getCurrentElementName() = 0;
		virtual void parserWarning(const std::string& message);
		virtual void parserError(const std::string& message);
		void setParseSilently(bool silent) { mParseSilently = silent; }

		S32 getParseGeneration() { return mParseGeneration; }
		S32 newParseGeneration() { return mParseGeneration = ++sNextParseGeneration; }


	protected:
		template <typename T>
		void registerParserFuncs(parser_read_func_t read_func, parser_write_func_t write_func = NULL)
		{
			mParserReadFuncs->insert(std::make_pair(&typeid(T), read_func));
			mParserWriteFuncs->insert(std::make_pair(&typeid(T), write_func));
		}

		template <typename T>
		void registerInspectFunc(parser_inspect_func_t inspect_func)
		{
			mParserInspectFuncs->insert(std::make_pair(&typeid(T), inspect_func));
		}

		bool				mParseSilently;

	private:
		parser_read_func_map_t*		mParserReadFuncs;
		parser_write_func_map_t*	mParserWriteFuncs;
		parser_inspect_func_map_t*	mParserInspectFuncs;
		S32	mParseGeneration;

		static S32					sNextParseGeneration;
	};

	// used to indicate no matching value to a given name when parsing
	struct NoParamValue{};

	class BaseBlock;

	class Param
	{
	public:
		// public to allow choice blocks to clear provided flag on stale choices
		void setProvided(bool is_provided) { mIsProvided = is_provided; }

	protected:
		bool anyProvided() const { return mIsProvided; }

		Param(class BaseBlock* enclosing_block);

		// store pointer to enclosing block as offset to reduce space and allow for quick copying
		class BaseBlock& enclosingBlock() const
		{ 
			const U8* my_addr = reinterpret_cast<const U8*>(this);
			// get address of enclosing BLOCK class using stored offset to enclosing BaseBlock class
			return *const_cast<class BaseBlock*>
				(reinterpret_cast<const class BaseBlock*>
					(my_addr - (ptrdiff_t)(S32)mEnclosingBlockOffset));
		}

	private:
		friend class BaseBlock;

		U16			mEnclosingBlockOffset;
		bool		mIsProvided;
	};

	// various callbacks and constraints associated with an individual param
	struct ParamDescriptor
	{
		struct UserData
		{
			virtual ~UserData() {}
		};

		typedef bool(*merge_func_t)(Param&, const Param&, bool);
		typedef bool(*deserialize_func_t)(Param&, Parser&, const Parser::name_stack_range_t&, S32);
		typedef void(*serialize_func_t)(const Param&, Parser&, Parser::name_stack_t&, const Param* diff_param);
		typedef void(*inspect_func_t)(const Param&, Parser&, Parser::name_stack_t&, S32 min_count, S32 max_count);
		typedef bool(*validation_func_t)(const Param*);

		ParamDescriptor(param_handle_t p, 
						merge_func_t merge_func, 
						deserialize_func_t deserialize_func, 
						serialize_func_t serialize_func,
						validation_func_t validation_func,
						inspect_func_t inspect_func,
						S32 min_count,
						S32 max_count);

		ParamDescriptor();
		~ParamDescriptor();

		param_handle_t		mParamHandle;
		merge_func_t		mMergeFunc;
		deserialize_func_t	mDeserializeFunc;
		serialize_func_t	mSerializeFunc;
		inspect_func_t		mInspectFunc;
		validation_func_t	mValidationFunc;
		S32					mMinCount;
		S32					mMaxCount;
		S32					mGeneration;
		S32					mNumRefs;
		UserData*			mUserData;
	};

	typedef boost::shared_ptr<ParamDescriptor> ParamDescriptorPtr;

	// each derived Block class keeps a static data structure maintaining offsets to various params
	class BlockDescriptor
	{
	public:
		BlockDescriptor();

		typedef enum e_initialization_state
		{
			UNINITIALIZED,
			INITIALIZING,
			INITIALIZED
		} EInitializationState;

		void aggregateBlockData(BlockDescriptor& src_block_data);

		typedef boost::unordered_map<const std::string, ParamDescriptorPtr>						param_map_t; 
		typedef std::vector<ParamDescriptorPtr>													param_list_t; 
		typedef std::list<ParamDescriptorPtr>														all_params_list_t;
		typedef std::vector<std::pair<param_handle_t, ParamDescriptor::validation_func_t> >		param_validation_list_t;

		param_map_t						mNamedParams;			// parameters with associated names
		param_list_t					mUnnamedParams;			// parameters with_out_ associated names
		param_validation_list_t			mValidationList;		// parameters that must be validated
		all_params_list_t				mAllParams;				// all parameters, owns descriptors

		size_t					mMaxParamOffset;

		EInitializationState	mInitializationState;	// whether or not static block data has been initialized
		class BaseBlock*		mCurrentBlockPtr;		// pointer to block currently being constructed
	};

	class BaseBlock
	{
	public:
		// "Multiple" constraint types, put here in root class to avoid ambiguity during use
		struct AnyAmount
		{
			static U32 minCount() { return 0; }
			static U32 maxCount() { return U32_MAX; }
		};

		template<U32 MIN_AMOUNT>
		struct AtLeast
		{
			static U32 minCount() { return MIN_AMOUNT; }
			static U32 maxCount() { return U32_MAX; }
		};

		template<U32 MAX_AMOUNT>
		struct AtMost
		{
			static U32 minCount() { return 0; }
			static U32 maxCount() { return MAX_AMOUNT; }
		};

		template<U32 MIN_AMOUNT, U32 MAX_AMOUNT>
		struct Between
		{
			static U32 minCount() { return MIN_AMOUNT; }
			static U32 maxCount() { return MAX_AMOUNT; }
		};

		template<U32 EXACT_COUNT>
		struct Exactly
		{
			static U32 minCount() { return EXACT_COUNT; }
			static U32 maxCount() { return EXACT_COUNT; }
		};

		// this typedef identifies derived classes as being blocks
		typedef void baseblock_base_class_t;
		LOG_CLASS(BaseBlock);
		friend class Param;

		BaseBlock();
		virtual ~BaseBlock();
		bool submitValue(const Parser::name_stack_t& name_stack, Parser& p, bool silent=false);

		param_handle_t getHandleFromParam(const Param* param) const;
		bool validateBlock(bool emit_errors = true) const;

		Param* getParamFromHandle(const param_handle_t param_handle)
		{
			if (param_handle == 0) return NULL;

			U8* baseblock_address = reinterpret_cast<U8*>(this);
			return reinterpret_cast<Param*>(baseblock_address + param_handle);
		}

		const Param* getParamFromHandle(const param_handle_t param_handle) const
		{
			const U8* baseblock_address = reinterpret_cast<const U8*>(this);
			return reinterpret_cast<const Param*>(baseblock_address + param_handle);
		}

		void addSynonym(Param& param, const std::string& synonym);

		// Blocks can override this to do custom tracking of changes
		virtual void paramChanged(const Param& changed_param, bool user_provided);

		S32 getLastChangeVersion() const { return mChangeVersion; }

		bool deserializeBlock(Parser& p, Parser::name_stack_range_t name_stack, S32 generation);
		void serializeBlock(Parser& p, Parser::name_stack_t name_stack = Parser::name_stack_t(), const BaseBlock* diff_block = NULL) const;
		bool inspectBlock(Parser& p, Parser::name_stack_t name_stack = Parser::name_stack_t(), S32 min_count = 0, S32 max_count = S32_MAX) const;

		virtual const BlockDescriptor& mostDerivedBlockDescriptor() const { return selfBlockDescriptor(); }
		virtual BlockDescriptor& mostDerivedBlockDescriptor() { return selfBlockDescriptor(); }

		// take all provided params from other and apply to self
		bool overwriteFrom(const BaseBlock& other)
		{
			return false;
		}

		// take all provided params that are not already provided, and apply to self
		bool fillFrom(const BaseBlock& other)
		{
			return false;
		}

		static void addParam(BlockDescriptor& block_data, ParamDescriptorPtr param, const char* name);

		ParamDescriptorPtr findParamDescriptor(const Param& param);

	protected:
		void init(BlockDescriptor& descriptor, BlockDescriptor& base_descriptor, size_t block_size);


		// take all provided params from other and apply to self
		bool merge(BlockDescriptor& block_data, const BaseBlock& other, bool overwrite);

		// can be updated in getters
		mutable S32				mChangeVersion;

		static BlockDescriptor& selfBlockDescriptor()
		{
			static BlockDescriptor sBlockDescriptor;
			return sBlockDescriptor;
		}

	private:
		const std::string& getParamName(const BlockDescriptor& block_data, const Param* paramp) const;
	};

	// these templates allow us to distinguish between template parameters
	// that derive from BaseBlock and those that don't
	template<typename T, typename Void = void>
	struct IsBlock
	{
		static const bool value = false;
	};

	template<typename T>
	struct IsBlock<T, typename T::baseblock_base_class_t>
	{
		static const bool value = true;
	};

	template<typename T, typename NAME_VALUE_LOOKUP, bool VALUE_IS_BLOCK = IsBlock<T>::value>
	class ParamValue : public NAME_VALUE_LOOKUP
	{
	public:
		typedef const T&							value_assignment_t;

		ParamValue(): mValue() {}
		ParamValue(const T& other) : mValue(other) {}

		void setValue(value_assignment_t val)
		{
			mValue = val;
		}

		value_assignment_t getValue() const
		{
			return mValue;
		}

		T& getValue()
		{
			return mValue;
		}

	private:
		T mValue;
	};

	template<typename T, typename NAME_VALUE_LOOKUP>
	class ParamValue<T, NAME_VALUE_LOOKUP, true> 
	:	public T,
		public NAME_VALUE_LOOKUP
	{
	public:
		typedef const T&							value_assignment_t;

		S32 			mKeyVersion;
		mutable S32 	mValidatedVersion;
		mutable bool 	mValidated; // lazy validation flag

		ParamValue() 
		:	T(),
			mKeyVersion(0),
			mValidatedVersion(-1),
			mValidated(false)
		{}

		ParamValue(const T& other)
		:	T(other),
			mKeyVersion(0),
			mValidatedVersion(-1),
			mValidated(false)
		{
		}

		void setValue(value_assignment_t val)
		{
			*this = val;
		}

		value_assignment_t getValue() const
		{
			return *this;
		}

		T& getValue()
		{
			return *this;
		}
	};

	template<typename T, typename NAME_VALUE_LOOKUP = TypeValues<T> >
	struct ParamIterator
	{
		typedef typename std::vector<ParamValue<T, NAME_VALUE_LOOKUP> >::const_iterator		const_iterator;
		typedef typename std::vector<ParamValue<T, NAME_VALUE_LOOKUP> >::iterator			iterator;
	};

	// specialize for custom parsing/decomposition of specific classes
	// e.g. TypedParam<LLRect> has left, top, right, bottom, etc...
	template<typename	T,
			typename	NAME_VALUE_LOOKUP = TypeValues<T>,
			bool		HAS_MULTIPLE_VALUES = false,
			bool		VALUE_IS_BLOCK = IsBlock<ParamValue<T, NAME_VALUE_LOOKUP> >::value>
	class TypedParam 
	:	public Param, 
		public ParamValue<T, NAME_VALUE_LOOKUP>
	{
	public:
		typedef const T&																	value_assignment_t;
		typedef	TypedParam<T, NAME_VALUE_LOOKUP, HAS_MULTIPLE_VALUES, VALUE_IS_BLOCK>		self_t;
		typedef NAME_VALUE_LOOKUP															name_value_lookup_t;
		typedef ParamValue<T, NAME_VALUE_LOOKUP>											param_value_t;

		TypedParam(BlockDescriptor& block_descriptor, const char* name, value_assignment_t value, ParamDescriptor::validation_func_t validate_func, S32 min_count, S32 max_count) 
		:	Param(block_descriptor.mCurrentBlockPtr)
		{
			if (LL_UNLIKELY(block_descriptor.mInitializationState == BlockDescriptor::INITIALIZING))
			{
 				ParamDescriptorPtr param_descriptor = ParamDescriptorPtr(new ParamDescriptor(
												block_descriptor.mCurrentBlockPtr->getHandleFromParam(this),
												&mergeWith,
												&deserializeParam,
												&serializeParam,
												validate_func,
												&inspectParam,
												min_count, max_count));
				BaseBlock::addParam(block_descriptor, param_descriptor, name);
			}

			setValue(value);
		} 

		bool isProvided() const { return Param::anyProvided(); }

		static bool deserializeParam(Param& param, Parser& parser, const Parser::name_stack_range_t& name_stack, S32 generation) 
		{ 
			self_t& typed_param = static_cast<self_t&>(param);
			// no further names in stack, attempt to parse value now
			if (name_stack.first == name_stack.second)
			{
				if (parser.readValue(typed_param.getValue()))
				{
					typed_param.clearValueName();
					typed_param.setProvided(true);
					typed_param.enclosingBlock().paramChanged(param, true);
					return true;
				}
				
				// try to parse a known named value
				if(name_value_lookup_t::valueNamesExist())
				{
					// try to parse a known named value
					std::string name;
					if (parser.readValue(name))
					{
						// try to parse a per type named value
						if (name_value_lookup_t::getValueFromName(name, typed_param.getValue()))
						{
							typed_param.setValueName(name);
							typed_param.setProvided(true);
							typed_param.enclosingBlock().paramChanged(param, true);
							return true;
						}

					}
				}
			}
			return false;
		}

		static void serializeParam(const Param& param, Parser& parser, Parser::name_stack_t& name_stack, const Param* diff_param)
		{
			const self_t& typed_param = static_cast<const self_t&>(param);
			if (!typed_param.isProvided()) return;

			if (!name_stack.empty())
			{
				name_stack.back().second = parser.newParseGeneration();
			}

			std::string key = typed_param.getValueName();

			// first try to write out name of name/value pair

			if (!key.empty())
			{
				if (!diff_param || !ParamCompare<std::string>::equals(static_cast<const self_t*>(diff_param)->getValueName(), key))
				{
					if (!parser.writeValue(key, name_stack))
					{
						return;
					}
				}
			}
			// then try to serialize value directly
			else if (!diff_param || !ParamCompare<T>::equals(typed_param.getValue(), static_cast<const self_t*>(diff_param)->getValue()))
			{
				if (!parser.writeValue(typed_param.getValue(), name_stack)) 
				{
					return;
				}
			}
		}

		static void inspectParam(const Param& param, Parser& parser, Parser::name_stack_t& name_stack, S32 min_count, S32 max_count)
		{
			// tell parser about our actual type
			parser.inspectValue<T>(name_stack, min_count, max_count, NULL);
			// then tell it about string-based alternatives ("red", "blue", etc. for LLColor4)
			if (name_value_lookup_t::getPossibleValues())
			{
				parser.inspectValue<std::string>(name_stack, min_count, max_count, name_value_lookup_t::getPossibleValues());
			}
		}

		void set(value_assignment_t val, bool flag_as_provided = true)
		{
			setValue(val);
			param_value_t::clearValueName();
			setProvided(flag_as_provided);
			Param::enclosingBlock().paramChanged(*this, flag_as_provided);
		}

		void setIfNotProvided(value_assignment_t val, bool flag_as_provided = true)
		{
			if (!isProvided())
			{
				set(val, flag_as_provided);
			}
		}

		// implicit conversion
		operator value_assignment_t() const { return param_value_t::getValue(); } 
		// explicit conversion
		value_assignment_t operator()() const { return param_value_t::getValue(); } 

	protected:

		static bool mergeWith(Param& dst, const Param& src, bool overwrite)
		{
			const self_t& src_typed_param = static_cast<const self_t&>(src);
			self_t& dst_typed_param = static_cast<self_t&>(dst);

			if (src_typed_param.isProvided()
				&& (overwrite || !dst_typed_param.isProvided()))
			{
				dst_typed_param.clearValueName();
				dst_typed_param.set(src_typed_param.getValue());
				return true;
			}
			return false;
		}
	};

	// parameter that is a block
	template <typename T, typename NAME_VALUE_LOOKUP>
	class TypedParam<T, NAME_VALUE_LOOKUP, false, true> 
	:	public Param,
		public ParamValue<T, NAME_VALUE_LOOKUP>
	{
	public:
		typedef const T											value_const_t;
		typedef T												value_t;
		typedef value_const_t&									value_assignment_t;
		typedef TypedParam<T, NAME_VALUE_LOOKUP, false, true>	self_t;
		typedef NAME_VALUE_LOOKUP								name_value_lookup_t;
		typedef ParamValue<T, NAME_VALUE_LOOKUP>				param_value_t;

		TypedParam(BlockDescriptor& block_descriptor, const char* name, value_assignment_t value, ParamDescriptor::validation_func_t validate_func, S32 min_count, S32 max_count)
		:	Param(block_descriptor.mCurrentBlockPtr),
			param_value_t(value)
		{
			if (LL_UNLIKELY(block_descriptor.mInitializationState == BlockDescriptor::INITIALIZING))
			{
				ParamDescriptorPtr param_descriptor = ParamDescriptorPtr(new ParamDescriptor(
												block_descriptor.mCurrentBlockPtr->getHandleFromParam(this),
												&mergeWith,
												&deserializeParam,
												&serializeParam,
												validate_func, 
												&inspectParam,
												min_count, max_count));
				BaseBlock::addParam(block_descriptor, param_descriptor, name);
			}
		}

		static bool deserializeParam(Param& param, Parser& parser, const Parser::name_stack_range_t& name_stack, S32 generation) 
		{ 
			self_t& typed_param = static_cast<self_t&>(param);
			// attempt to parse block...
			if(typed_param.deserializeBlock(parser, name_stack, generation))
			{
				typed_param.clearValueName();
				typed_param.enclosingBlock().paramChanged(param, true);
				typed_param.setProvided(true);
				return true;
			}

			if(name_value_lookup_t::valueNamesExist())
			{
				// try to parse a known named value
				std::string name;
				if (parser.readValue(name))
				{
					// try to parse a per type named value
					if (name_value_lookup_t::getValueFromName(name, typed_param.getValue()))
					{
						typed_param.enclosingBlock().paramChanged(param, true);
						typed_param.setValueName(name);
						typed_param.setProvided(true);
						typed_param.mKeyVersion = typed_param.getLastChangeVersion();
						return true;
					}

				}
			}
			return false;
		}

		static void serializeParam(const Param& param, Parser& parser, Parser::name_stack_t& name_stack, const Param* diff_param)
		{
			const self_t& typed_param = static_cast<const self_t&>(param);
			if (!typed_param.isProvided()) return;

			if (!name_stack.empty())
			{
				name_stack.back().second = parser.newParseGeneration();
			}

			std::string key = typed_param.getValueName();
			if (!key.empty() && typed_param.mKeyVersion == typed_param.getLastChangeVersion())
			{
				if (!parser.writeValue(key, name_stack))
				{
					return;
				}
			}
			else
			{
				typed_param.serializeBlock(parser, name_stack, static_cast<const self_t*>(diff_param));
			}
		}

		static void inspectParam(const Param& param, Parser& parser, Parser::name_stack_t& name_stack, S32 min_count, S32 max_count)
		{
			// I am a param that is also a block, so just recurse into my contents
			const self_t& typed_param = static_cast<const self_t&>(param);
			typed_param.inspectBlock(parser, name_stack, min_count, max_count);
		}

		// a param-that-is-a-block is provided when the user has set one of its child params
		// *and* the block as a whole validates
		bool isProvided() const 
		{ 
			// only validate block when it hasn't already passed validation with current data
			if (Param::anyProvided() && param_value_t::mValidatedVersion < param_value_t::getLastChangeVersion())
			{
				// a sub-block is "provided" when it has been filled in enough to be valid
				param_value_t::mValidated = param_value_t::validateBlock(false);
				param_value_t::mValidatedVersion = param_value_t::getLastChangeVersion();
			}
			return Param::anyProvided() && param_value_t::mValidated;
		}

		// assign block contents to this param-that-is-a-block
		void set(value_assignment_t val, bool flag_as_provided = true)
		{
			setValue(val);
			param_value_t::clearValueName();
			// force revalidation of block by clearing known provided version
			// next call to isProvided() will update provision status based on validity
			param_value_t::mValidatedVersion = -1;
			setProvided(flag_as_provided);
			Param::enclosingBlock().paramChanged(*this, flag_as_provided);
		}

		void setIfNotProvided(value_assignment_t val, bool flag_as_provided = true)
		{
			if (!isProvided())
			{
				set(val, flag_as_provided);
			}
		}

		// propagate changed status up to enclosing block
		/*virtual*/ void paramChanged(const Param& changed_param, bool user_provided)
		{ 
			ParamValue<T, NAME_VALUE_LOOKUP>::paramChanged(changed_param, user_provided);
			Param::enclosingBlock().paramChanged(*this, user_provided);
			if (user_provided)
			{
				// a child param has been explicitly changed
				// so *some* aspect of this block is now provided
				setProvided(true);
			}
		}

		// implicit conversion
		operator value_assignment_t() const { return param_value_t::getValue(); } 
		// explicit conversion
		value_assignment_t operator()() const { return param_value_t::getValue(); } 

	protected:

		static bool mergeWith(Param& dst, const Param& src, bool overwrite)
		{
			const self_t& src_typed_param = static_cast<const self_t&>(src);
			self_t& dst_typed_param = static_cast<self_t&>(dst);

			if (src_typed_param.isProvided()
				&& (overwrite || !dst_typed_param.isProvided()))
			{
				if (dst_typed_param.merge(param_value_t::selfBlockDescriptor(), src_typed_param, overwrite))
				{
					dst_typed_param.clearValueName();
					return true;
				}
			}
			return false;
		}
	};

	// container of non-block parameters
	template <typename VALUE_TYPE, typename NAME_VALUE_LOOKUP>
	class TypedParam<VALUE_TYPE, NAME_VALUE_LOOKUP, true, false> 
	:	public Param
	{
	public:
		typedef TypedParam<VALUE_TYPE, NAME_VALUE_LOOKUP, true, false>		self_t;
		typedef ParamValue<VALUE_TYPE, NAME_VALUE_LOOKUP>			param_value_t;
		typedef typename std::vector<param_value_t>							container_t;
		typedef const container_t&											value_assignment_t;

		typedef VALUE_TYPE													value_t;
		typedef NAME_VALUE_LOOKUP											name_value_lookup_t;
		
		TypedParam(BlockDescriptor& block_descriptor, const char* name, value_assignment_t value, ParamDescriptor::validation_func_t validate_func, S32 min_count, S32 max_count) 
		:	Param(block_descriptor.mCurrentBlockPtr)
		{
			std::copy(value.begin(), value.end(), std::back_inserter(mValues));

			if (LL_UNLIKELY(block_descriptor.mInitializationState == BlockDescriptor::INITIALIZING))
			{
				ParamDescriptorPtr param_descriptor = ParamDescriptorPtr(new ParamDescriptor(
												block_descriptor.mCurrentBlockPtr->getHandleFromParam(this),
												&mergeWith,
												&deserializeParam,
												&serializeParam,
												validate_func,
												&inspectParam,
												min_count, max_count));
				BaseBlock::addParam(block_descriptor, param_descriptor, name);
			}
		} 

		bool isProvided() const { return Param::anyProvided(); }

		static bool deserializeParam(Param& param, Parser& parser, const Parser::name_stack_range_t& name_stack, S32 generation) 
		{ 
			self_t& typed_param = static_cast<self_t&>(param);
			value_t value;
			// no further names in stack, attempt to parse value now
			if (name_stack.first == name_stack.second)
			{
				// attempt to read value directly
				if (parser.readValue(value))
				{
					typed_param.add(value);
					return true;
				}
				
				// try to parse a known named value
				if(name_value_lookup_t::valueNamesExist())
				{
					// try to parse a known named value
					std::string name;
					if (parser.readValue(name))
					{
						// try to parse a per type named value
						if (name_value_lookup_t::getValueFromName(name, typed_param.mValues))
						{
							typed_param.add(value);
							typed_param.mValues.back().setValueName(name);
							return true;
						}

					}
				}
			}
			return false;
		}

		static void serializeParam(const Param& param, Parser& parser, Parser::name_stack_t& name_stack, const Param* diff_param)
		{
			const self_t& typed_param = static_cast<const self_t&>(param);
			if (!typed_param.isProvided() || name_stack.empty()) return;

			for (const_iterator it = typed_param.mValues.begin(), end_it = typed_param.mValues.end();
				it != end_it;
				++it)
			{
				std::string key = it->getValue();
				name_stack.back().second = parser.newParseGeneration();

				if(key.empty())
				// not parsed via name values, write out value directly
				{
					if (!parser.writeValue(*it, name_stack))
					{
						break;
					}
				}
				else 
				{
					if(!parser.writeValue(key, name_stack))
					{
						break;
					}
				}
			}
		}

		static void inspectParam(const Param& param, Parser& parser, Parser::name_stack_t& name_stack, S32 min_count, S32 max_count)
		{
			parser.inspectValue<VALUE_TYPE>(name_stack, min_count, max_count, NULL);
			if (name_value_lookup_t::getPossibleValues())
			{
				parser.inspectValue<std::string>(name_stack, min_count, max_count, name_value_lookup_t::getPossibleValues());
			}
		}

		void set(value_assignment_t val, bool flag_as_provided = true)
		{
			mValues = val;
			setProvided(flag_as_provided);
			Param::enclosingBlock().paramChanged(*this, flag_as_provided);
		}


		void setIfNotProvided(value_assignment_t val, bool flag_as_provided = true)
		{
			if (!isProvided())
			{
				set(val, flag_as_provided);
			}
		}

		value_t& add()
		{
			mValues.push_back(param_value_t(value_t()));
			setProvided(true);
			Param::enclosingBlock().paramChanged(*this, true);
			return mValues.back();
		}

		void add(const value_t& item)
		{
			mValues.push_back(param_value_t(item));
			setProvided(true);
			Param::enclosingBlock().paramChanged(*this, true);
		}

		// implicit conversion
		operator value_assignment_t() const { return mValues; } 

		typedef typename container_t::iterator iterator;
		typedef typename container_t::const_iterator const_iterator;
		iterator begin() { return mValues.begin(); }
		iterator end() { return mValues.end(); }
		const_iterator begin() const { return mValues.begin(); }
		const_iterator end() const { return mValues.end(); }
		bool empty() const { return mValues.empty(); }
		size_t size() const { return mValues.size(); }

		U32 numValidElements() const
		{
			return mValues.size();
		}

	protected:
		static bool mergeWith(Param& dst, const Param& src, bool overwrite)
		{
			const self_t& src_typed_param = static_cast<const self_t&>(src);
			self_t& dst_typed_param = static_cast<self_t&>(dst);

			if (overwrite)
			{
				std::copy(src_typed_param.begin(), src_typed_param.end(), std::back_inserter(dst_typed_param.mValues));
			}
			else
			{
				container_t new_values(src_typed_param.mValues);
				std::copy(dst_typed_param.begin(), dst_typed_param.end(), std::back_inserter(new_values));
				std::swap(dst_typed_param.mValues, new_values);
			}
			return true;
		}

		container_t		mValues;
	};

	// container of block parameters
	template <typename VALUE_TYPE, typename NAME_VALUE_LOOKUP>
	class TypedParam<VALUE_TYPE, NAME_VALUE_LOOKUP, true, true> 
	:	public Param
	{
	public:
		typedef TypedParam<VALUE_TYPE, NAME_VALUE_LOOKUP, true, true>	self_t;
		typedef ParamValue<VALUE_TYPE, NAME_VALUE_LOOKUP>				param_value_t;
		typedef typename std::vector<param_value_t>						container_t;
		typedef const container_t&										value_assignment_t;
		typedef VALUE_TYPE												value_t;
		typedef NAME_VALUE_LOOKUP										name_value_lookup_t;

		TypedParam(BlockDescriptor& block_descriptor, const char* name, value_assignment_t value, ParamDescriptor::validation_func_t validate_func, S32 min_count, S32 max_count) 
		:	Param(block_descriptor.mCurrentBlockPtr),
			mLastParseGeneration(0)
		{
			std::copy(value.begin(), value.end(), back_inserter(mValues));

			if (LL_UNLIKELY(block_descriptor.mInitializationState == BlockDescriptor::INITIALIZING))
			{
				ParamDescriptorPtr param_descriptor = ParamDescriptorPtr(new ParamDescriptor(
												block_descriptor.mCurrentBlockPtr->getHandleFromParam(this),
												&mergeWith,
												&deserializeParam,
												&serializeParam,
												validate_func,
												&inspectParam,
												min_count, max_count));
				BaseBlock::addParam(block_descriptor, param_descriptor, name);
			}
		} 

		bool isProvided() const { return Param::anyProvided(); }

		static bool deserializeParam(Param& param, Parser& parser, const Parser::name_stack_range_t& name_stack, S32 generation) 
		{ 
			self_t& typed_param = static_cast<self_t&>(param);
			bool new_value = false;

			if (generation != typed_param.mLastParseGeneration 
				|| typed_param.mValues.empty())
			{
				new_value = true;
				typed_param.mValues.push_back(value_t());
			}

			param_value_t& value = typed_param.mValues.back();

			// attempt to parse block...
			if(value.deserializeBlock(parser, name_stack, generation))
			{
				if (new_value)
				{	// successfully parsed new value, let's keep it
					typed_param.mLastParseGeneration = generation;
				}
				typed_param.enclosingBlock().paramChanged(param, true);
				typed_param.setProvided(true);
				return true;
			}
			else if(name_value_lookup_t::valueNamesExist())
			{
				// try to parse a known named value
				std::string name;
				if (parser.readValue(name))
				{
					// try to parse a per type named value
					if (name_value_lookup_t::getValueFromName(name, value.getValue()))
					{
						if (new_value)
						{	// successfully parsed new value, let's keep it
							typed_param.mLastParseGeneration = generation;
						}

						typed_param.mValues.back().setValueName(name);
						typed_param.mValues.back().mKeyVersion = value.getLastChangeVersion();
						typed_param.enclosingBlock().paramChanged(param, true);
						typed_param.setProvided(true);
						return true;
					}

				}
			}

			if (new_value)
			{	// failed to parse new value, pop it off
				typed_param.mValues.pop_back();
			}

			return false;
		}

		static void serializeParam(const Param& param, Parser& parser, Parser::name_stack_t& name_stack, const Param* diff_param)
		{
			const self_t& typed_param = static_cast<const self_t&>(param);
			if (!typed_param.isProvided() || name_stack.empty()) return;

			for (const_iterator it = typed_param.mValues.begin(), end_it = typed_param.mValues.end();
				it != end_it;
				++it)
			{
				name_stack.back().second = parser.newParseGeneration();

				std::string key = it->getValueName();
				if (!key.empty() && it->mKeyVersion == it->getLastChangeVersion())
				{
					parser.writeValue(key, name_stack);
				}
				// Not parsed via named values, write out value directly
				// NOTE: currently we don't worry about removing default values in Multiple
				else 
				{
					it->serializeBlock(parser, name_stack, NULL);
				}
			}
		}

		static void inspectParam(const Param& param, Parser& parser, Parser::name_stack_t& name_stack, S32 min_count, S32 max_count)
		{
			// I am a vector of blocks, so describe my contents recursively
			param_value_t(value_t()).inspectBlock(parser, name_stack, min_count, max_count);
		}

		void set(value_assignment_t val, bool flag_as_provided = true)
		{
			mValues = val;
			setProvided(flag_as_provided);
			Param::enclosingBlock().paramChanged(*this, flag_as_provided);
		}

		void setIfNotProvided(value_assignment_t val, bool flag_as_provided = true)
		{
			if (!isProvided())
			{
				set(val, flag_as_provided);
			}
		}

		value_t& add()
		{
			mValues.push_back(value_t());
			setProvided(true);
			Param::enclosingBlock().paramChanged(*this, true);
			return mValues.back();
		}

		void add(const value_t& item)
		{
			mValues.push_back(item);
			setProvided(true);
			Param::enclosingBlock().paramChanged(*this, true);
		}

		// implicit conversion
		operator value_assignment_t() const { return mValues; } 

		typedef typename container_t::iterator iterator;
		typedef typename container_t::const_iterator const_iterator;
		iterator begin() { return mValues.begin(); }
		iterator end() { return mValues.end(); }
		const_iterator begin() const { return mValues.begin(); }
		const_iterator end() const { return mValues.end(); }
		bool empty() const { return mValues.empty(); }
		size_t size() const { return mValues.size(); }

		U32 numValidElements() const
		{
			U32 count = 0;
			for (const_iterator it = mValues.begin(), end_it = mValues.end();
				it != end_it;
				++it)
			{
				if(it->validateBlock(false)) count++;
			}
			return count;
		}

	protected:

		static bool mergeWith(Param& dst, const Param& src, bool overwrite)
		{
			const self_t& src_typed_param = static_cast<const self_t&>(src);
			self_t& dst_typed_param = static_cast<self_t&>(dst);

			if (overwrite)
			{
				std::copy(src_typed_param.begin(), src_typed_param.end(), std::back_inserter(dst_typed_param.mValues));
			}
			else
			{
				container_t new_values(src_typed_param.mValues);
				std::copy(dst_typed_param.begin(), dst_typed_param.end(), std::back_inserter(new_values));
				std::swap(dst_typed_param.mValues, new_values);
			}
			return true;
		}

		container_t			mValues;

		S32			mLastParseGeneration;
	};

	template <typename DERIVED_BLOCK>
	class Choice : public BaseBlock
	{
		typedef Choice<DERIVED_BLOCK>	self_t;
		typedef Choice<DERIVED_BLOCK>	enclosing_block_t;
		
		LOG_CLASS(self_t);
	public:
		// take all provided params from other and apply to self
		bool overwriteFrom(const self_t& other)
		{
			return merge(selfBlockDescriptor(), other, true);
		}

		// take all provided params that are not already provided, and apply to self
		bool fillFrom(const self_t& other)
		{
			return merge(selfBlockDescriptor(), other, false);
		}

		// merge with other block
		bool merge(BlockDescriptor& block_data, const self_t& other, bool overwrite)
		{
			// only merge a choice if we are overwriting with other's contents
			if (overwrite)
			{
				mCurChoice = other.mCurChoice;
				return BaseBlock::merge(selfBlockDescriptor(), other, overwrite);
			}
			return false;
		}

		// clear out old choice when param has changed
		/*virtual*/ void paramChanged(const Param& changed_param, bool user_provided)
		{ 
			param_handle_t changed_param_handle = BaseBlock::getHandleFromParam(&changed_param);
			// if we have a new choice...
			if (changed_param_handle != mCurChoice)
			{
				// clear provided flag on previous choice
				Param* previous_choice = BaseBlock::getParamFromHandle(mCurChoice);
				if (previous_choice) 
				{
					previous_choice->setProvided(false);
				}
				mCurChoice = changed_param_handle;
			}
			BaseBlock::paramChanged(changed_param, user_provided);
		}

		virtual const BlockDescriptor& mostDerivedBlockDescriptor() const { return selfBlockDescriptor(); }
		virtual BlockDescriptor& mostDerivedBlockDescriptor() { return selfBlockDescriptor(); }

	protected:
		Choice()
		:	mCurChoice(0)
		{
			BaseBlock::init(selfBlockDescriptor(), BaseBlock::selfBlockDescriptor(), sizeof(DERIVED_BLOCK));
		}

		// Alternatives are mutually exclusive wrt other Alternatives in the same block.  
		// One alternative in a block will always have isChosen() == true.
		// At most one alternative in a block will have isProvided() == true.
		template <typename T, typename NAME_VALUE_LOOKUP = TypeValues<T> >
		class Alternative : public TypedParam<T, NAME_VALUE_LOOKUP, false>
		{
		public:
			friend class Choice<DERIVED_BLOCK>;

			typedef Alternative<T, NAME_VALUE_LOOKUP>									self_t;
			typedef TypedParam<T, NAME_VALUE_LOOKUP, false, IsBlock<ParamValue<T, NAME_VALUE_LOOKUP> >::value>		super_t;
			typedef typename super_t::value_assignment_t								value_assignment_t;

			explicit Alternative(const char* name, value_assignment_t val = defaultValue<T>())
			:	super_t(DERIVED_BLOCK::selfBlockDescriptor(), name, val, NULL, 0, 1),
				mOriginalValue(val)
			{
				// assign initial choice to first declared option
				DERIVED_BLOCK* blockp = ((DERIVED_BLOCK*)DERIVED_BLOCK::selfBlockDescriptor().mCurrentBlockPtr);
				if (LL_UNLIKELY(DERIVED_BLOCK::selfBlockDescriptor().mInitializationState == BlockDescriptor::INITIALIZING))
				{
					if(blockp->mCurChoice == 0)
					{
						blockp->mCurChoice = Param::enclosingBlock().getHandleFromParam(this);
					}
				}
			}

			Alternative& operator=(value_assignment_t val)
			{
				super_t::set(val);
				return *this;
			}

			void operator()(typename super_t::value_assignment_t val) 
			{ 
				super_t::set(val);
			}

			operator value_assignment_t() const 
			{ 
				if (static_cast<enclosing_block_t&>(Param::enclosingBlock()).getCurrentChoice() == this)
				{
					return super_t::getValue(); 
				}
				return mOriginalValue;
			} 

			value_assignment_t operator()() const 
			{ 
				if (static_cast<enclosing_block_t&>(Param::enclosingBlock()).getCurrentChoice() == this)
				{
					return super_t::getValue(); 
				}
				return mOriginalValue;
			} 

			bool isChosen() const
			{
				return static_cast<enclosing_block_t&>(Param::enclosingBlock()).getCurrentChoice() == this;
			}
		
		private:
			T			mOriginalValue;
		};

	protected:
		static BlockDescriptor& selfBlockDescriptor()
		{
			static BlockDescriptor sBlockDescriptor;
			return sBlockDescriptor;
		}

	private:
		param_handle_t	mCurChoice;

		const Param* getCurrentChoice() const
		{
			return BaseBlock::getParamFromHandle(mCurChoice);
		}
	};

	template <typename DERIVED_BLOCK, typename BASE_BLOCK = BaseBlock>
	class Block 
	:	public BASE_BLOCK
	{
		typedef Block<DERIVED_BLOCK, BASE_BLOCK>	self_t;
		typedef Block<DERIVED_BLOCK, BASE_BLOCK>	block_t;

	public:
		typedef BASE_BLOCK base_block_t;

		// take all provided params from other and apply to self
		bool overwriteFrom(const self_t& other)
		{
			return static_cast<DERIVED_BLOCK*>(this)->merge(selfBlockDescriptor(), other, true);
		}

		// take all provided params that are not already provided, and apply to self
		bool fillFrom(const self_t& other)
		{
			return static_cast<DERIVED_BLOCK*>(this)->merge(selfBlockDescriptor(), other, false);
		}

		virtual const BlockDescriptor& mostDerivedBlockDescriptor() const { return selfBlockDescriptor(); }
		virtual BlockDescriptor& mostDerivedBlockDescriptor() { return selfBlockDescriptor(); }

	protected:
		Block()
		{
			//#pragma message("Parsing LLInitParam::Block")
			BaseBlock::init(selfBlockDescriptor(), BASE_BLOCK::selfBlockDescriptor(), sizeof(DERIVED_BLOCK));
		}

		//
		// Nested classes for declaring parameters
		//
		template <typename T, typename NAME_VALUE_LOOKUP = TypeValues<T> >
		class Optional : public TypedParam<T, NAME_VALUE_LOOKUP, false>
		{
		public:
			typedef TypedParam<T, NAME_VALUE_LOOKUP, false, IsBlock<ParamValue<T, NAME_VALUE_LOOKUP> >::value>		super_t;
			typedef typename super_t::value_assignment_t								value_assignment_t;

			explicit Optional(const char* name = "", value_assignment_t val = defaultValue<T>())
			:	super_t(DERIVED_BLOCK::selfBlockDescriptor(), name, val, NULL, 0, 1)
			{
				//#pragma message("Parsing LLInitParam::Block::Optional")
			}

			Optional& operator=(value_assignment_t val)
			{
				set(val);
				return *this;
			}

			DERIVED_BLOCK& operator()(value_assignment_t val)
			{
				super_t::set(val);
				return static_cast<DERIVED_BLOCK&>(Param::enclosingBlock());
			}
			using super_t::operator();
		};

		template <typename T, typename NAME_VALUE_LOOKUP = TypeValues<T> >
		class Mandatory : public TypedParam<T, NAME_VALUE_LOOKUP, false>
		{
		public:
			typedef TypedParam<T, NAME_VALUE_LOOKUP, false, IsBlock<ParamValue<T, NAME_VALUE_LOOKUP> >::value>		super_t;
			typedef Mandatory<T, NAME_VALUE_LOOKUP>										self_t;
			typedef typename super_t::value_assignment_t								value_assignment_t;

			// mandatory parameters require a name to be parseable
			explicit Mandatory(const char* name = "", value_assignment_t val = defaultValue<T>())
			:	super_t(DERIVED_BLOCK::selfBlockDescriptor(), name, val, &validate, 1, 1)
			{}

			Mandatory& operator=(value_assignment_t val)
			{
				set(val);
				return *this;
			}

			DERIVED_BLOCK& operator()(typename super_t::value_assignment_t val)
			{
				super_t::set(val);
				return static_cast<DERIVED_BLOCK&>(Param::enclosingBlock());
			}
			using super_t::operator();

			static bool validate(const Param* p)
			{
				// valid only if provided
				return static_cast<const self_t*>(p)->isProvided();
			}

		};

		template <typename T, typename RANGE = BaseBlock::AnyAmount, typename NAME_VALUE_LOOKUP = TypeValues<T> >
		class Multiple : public TypedParam<T, NAME_VALUE_LOOKUP, true>
		{
		public:
			typedef TypedParam<T, NAME_VALUE_LOOKUP, true, IsBlock<ParamValue<T, NAME_VALUE_LOOKUP> >::value>	super_t;
			typedef Multiple<T, RANGE, NAME_VALUE_LOOKUP>							self_t;
			typedef typename super_t::container_t									container_t;
			typedef typename super_t::value_assignment_t							value_assignment_t;
			typedef typename super_t::iterator										iterator;
			typedef typename super_t::const_iterator								const_iterator;

			explicit Multiple(const char* name = "")
			:	super_t(DERIVED_BLOCK::selfBlockDescriptor(), name, container_t(), &validate, RANGE::minCount(), RANGE::maxCount())
			{}

			Multiple& operator=(value_assignment_t val)
			{
				set(val);
				return *this;
			}

			DERIVED_BLOCK& operator()(typename super_t::value_assignment_t val)
			{
				super_t::set(val);
				return static_cast<DERIVED_BLOCK&>(Param::enclosingBlock());
			}

			static bool validate(const Param* paramp) 
			{
				U32 num_valid = ((super_t*)paramp)->numValidElements();
				return RANGE::minCount() <= num_valid && num_valid <= RANGE::maxCount();
			}
		};

		template <typename T, typename RANGE = BaseBlock::AnyAmount, typename NAME_VALUE_LOOKUP = TypeValues<T> >
		class Batch : private TypedParam<T, NAME_VALUE_LOOKUP, false>
		{
		public:
			typedef ParamValue<T, NAME_VALUE_LOOKUP>										param_value_t;
			typedef TypedParam<T, NAME_VALUE_LOOKUP, false, IsBlock<param_value_t>::value>	super_t;
			typedef Batch<T, RANGE, NAME_VALUE_LOOKUP>										self_t;
			typedef typename super_t::value_assignment_t									value_assignment_t;
			typedef typename super_t::value_t												value_t;

			struct BatchDefaultValue : public ParamDescriptor::UserData
			{
				BatchDefaultValue(const T& value)
				:	mValue(value)
				{}

				T mValue;
			};

			explicit Batch(const char* name, value_assignment_t val)
			:	super_t(DERIVED_BLOCK::selfBlockDescriptor(), name, val, NULL, 0, 1),
				mLastParseGeneration(-1)
			{
				BlockDescriptor& block_descriptor = DERIVED_BLOCK::selfBlockDescriptor();
				if (LL_UNLIKELY(block_descriptor.mInitializationState == BlockDescriptor::INITIALIZING))
				{
					ParamDescriptorPtr param_descriptorp = block_descriptor.mCurrentBlockPtr->findParamDescriptor(*this);

					if (param_descriptorp)
					{
						param_descriptorp->mDeserializeFunc = &deserializeParam;
						param_descriptorp->mUserData = new BatchDefaultValue(new param_value_t(val));
					}
				}
			}

			explicit Batch(const char* name = "")
			:	super_t(DERIVED_BLOCK::selfBlockDescriptor(), name, defaultValue<T>(), NULL, 0, 1),
				mLastParseGeneration(-1)
			{
				BlockDescriptor& block_descriptor = DERIVED_BLOCK::selfBlockDescriptor();
				if (LL_UNLIKELY(block_descriptor.mInitializationState == BlockDescriptor::INITIALIZING))
				{
					ParamDescriptorPtr param_descriptorp = block_descriptor.mCurrentBlockPtr->findParamDescriptor(*this);

					if (param_descriptorp)
					{
						param_descriptorp->mDeserializeFunc = &deserializeParam;
					}
				}
			}

			Batch& operator=(value_assignment_t val)
			{
				set(val);
				return *this;
			}

			DERIVED_BLOCK& operator()(value_assignment_t val)
			{
				super_t::set(val);
				return static_cast<DERIVED_BLOCK&>(Param::enclosingBlock());
			}

			using super_t::operator();

		private:
			static bool deserializeParam(Param& param, Parser& parser, const Parser::name_stack_range_t& name_stack, S32 generation) 
			{
				self_t& typed_param = static_cast<self_t&>(param);

				if (generation != typed_param.mLastParseGeneration)
				{
					ParamDescriptorPtr descriptor = typed_param.enclosingBlock().findParamDescriptor(param);
					if (descriptor && static_cast<BatchDefaultValue*>(descriptor->mUserData))
					{
						static_cast<param_value_t&>(typed_param) = (static_cast<BatchDefaultValue*>(descriptor->mUserData))->mValue;
					}
					else
					{
						static_cast<param_value_t&>(typed_param) = param_value_t(value_t());
					}
					typed_param.mLastParseGeneration = generation;
				}
				return super_t::deserializeParam(param, parser, name_stack, generation);
			}

			S32 mLastParseGeneration;
		};

		class Deprecated : public Param
		{
		public:
			explicit Deprecated(const char* name)
			:	Param(DERIVED_BLOCK::selfBlockDescriptor().mCurrentBlockPtr)
			{
				BlockDescriptor& block_descriptor = DERIVED_BLOCK::selfBlockDescriptor();
				if (LL_UNLIKELY(block_descriptor.mInitializationState == BlockDescriptor::INITIALIZING))
				{
					ParamDescriptorPtr param_descriptor = ParamDescriptorPtr(new ParamDescriptor(
													block_descriptor.mCurrentBlockPtr->getHandleFromParam(this),
													NULL,
													&deserializeParam,
													NULL,
													NULL,
													NULL, 
													0, S32_MAX));
					BaseBlock::addParam(block_descriptor, param_descriptor, name);
				}
			}
			
			static bool deserializeParam(Param& param, Parser& parser, const Parser::name_stack_range_t& name_stack, S32 generation)
			{
				if (name_stack.first == name_stack.second)
				{
					//std::string message = llformat("Deprecated value %s ignored", getName().c_str());
					//parser.parserWarning(message);
					return true;
				}

				return false;
			}
		};

		typedef Deprecated Ignored;

	protected:
		static BlockDescriptor& selfBlockDescriptor()
		{
			static BlockDescriptor sBlockDescriptor;
			return sBlockDescriptor;
		}
	};
	
	template<typename T>
	class CustomParamValue
	:	public Block<ParamValue<T, TypeValues<T> > >,
		public TypeValues<T>
	{
	public:
		typedef enum e_value_age
		{	
			VALUE_NEEDS_UPDATE,		// mValue needs to be refreshed from the block parameters
			VALUE_AUTHORITATIVE,	// mValue holds the authoritative value (which has been replicated to the block parameters via updateBlockFromValue)
			BLOCK_AUTHORITATIVE		// mValue is derived from the block parameters, which are authoritative
		} EValueAge;

		typedef ParamValue<T, TypeValues<T> >	derived_t;
		typedef CustomParamValue<T>				self_t;
		typedef Block<derived_t>		block_t;
		typedef const T&						value_assignment_t;

		CustomParamValue(const T& value = T())
		:	mValue(value),
			mValueAge(VALUE_AUTHORITATIVE),
			mKeyVersion(0),
			mValidatedVersion(-1)
		{}

		bool deserializeBlock(Parser& parser, Parser::name_stack_range_t name_stack, S32 generation)
		{
			derived_t& typed_param = static_cast<derived_t&>(*this);
			// type to apply parse direct value T
			if (name_stack.first == name_stack.second)
			{
				if(parser.readValue(typed_param.mValue))
				{
					typed_param.clearValueName();
					typed_param.mValueAge = VALUE_AUTHORITATIVE;
					typed_param.updateBlockFromValue();

					return true;
				}
			}

			// fall back on parsing block components for T
			// if we deserialized at least one component...
			if (typed_param.BaseBlock::deserializeBlock(parser, name_stack, generation))
			{
				return true;
			}

			return false;
		}

		void serializeBlock(Parser& parser, Parser::name_stack_t name_stack = Parser::name_stack_t(), const BaseBlock* diff_block = NULL) const
		{
			const self_t& typed_param = static_cast<const self_t&>(*this);
			const self_t* diff_param = static_cast<const self_t*>(diff_block);
			
			std::string key = typed_param.getValueName();

			// first try to write out name of name/value pair
			if (!key.empty())
			{
				if (!diff_param || !ParamCompare<std::string>::equals(diff_param->getValueName(), key))
				{
					parser.writeValue(key, name_stack);
				}
			}
			// then try to serialize value directly
			else if (!diff_param || !ParamCompare<T>::equals(typed_param.getValue(), diff_param->getValue()))
            {
				
				if (!parser.writeValue(typed_param.getValue(), name_stack)) 
				{
					//RN: *always* serialize provided components of BlockValue (don't pass diff_param on),
					// since these tend to be viewed as the constructor arguments for the value T.  It seems
					// cleaner to treat the uniqueness of a BlockValue according to the generated value, and
					// not the individual components.  This way <color red="0" green="1" blue="0"/> will not
					// be exported as <color green="1"/>, since it was probably the intent of the user to 
					// be specific about the RGB color values.  This also fixes an issue where we distinguish
					// between rect.left not being provided and rect.left being explicitly set to 0 (same as default)
					block_t::serializeBlock(parser, name_stack, NULL);
				}
			}
		}

		bool inspectBlock(Parser& parser, Parser::name_stack_t name_stack = Parser::name_stack_t(), S32 min_count = 0, S32 max_count = S32_MAX) const
		{
			// first, inspect with actual type...
			parser.inspectValue<T>(name_stack, min_count, max_count, NULL);
			if (TypeValues<T>::getPossibleValues())
			{
				//...then inspect with possible string values...
				parser.inspectValue<std::string>(name_stack, min_count, max_count, TypeValues<T>::getPossibleValues());
			}
			// then recursively inspect contents...
			return block_t::inspectBlock(parser, name_stack, min_count, max_count);
		}

		bool validateBlock(bool emit_errors = true) const
		{
			if (mValueAge == VALUE_NEEDS_UPDATE)
			{
				if (block_t::validateBlock(emit_errors))
				{
					// clear stale keyword associated with old value
					TypeValues<T>::clearValueName();
					mValueAge = BLOCK_AUTHORITATIVE;
					static_cast<derived_t*>(const_cast<self_t*>(this))->updateValueFromBlock();
					return true;
				}
				else
				{
					//block value incomplete, so not considered provided
					// will attempt to revalidate on next call to isProvided()
					return false;  
				}
			}
			else
			{
				// we have a valid value in hand
				return true;
			}
		}

 		// propagate change status up to enclosing block
		/*virtual*/ void paramChanged(const Param& changed_param, bool user_provided)
		{ 
			BaseBlock::paramChanged(changed_param, user_provided);
			if (user_provided)
		{
				// a parameter changed, so our value is out of date
				mValueAge = VALUE_NEEDS_UPDATE;
			}
		}
			
		void setValue(value_assignment_t val)
		{
			derived_t& typed_param = static_cast<derived_t&>(*this);
			// set param version number to be up to date, so we ignore block contents
			mValueAge = VALUE_AUTHORITATIVE;
			mValue = val;
			typed_param.clearValueName();
			static_cast<derived_t*>(const_cast<self_t*>(this))->updateBlockFromValue();
		}

		value_assignment_t getValue() const
		{
			validateBlock(true);
			return mValue;
		}

		T& getValue() 
		{
			validateBlock(true);
			return mValue;
		}

		S32 				mKeyVersion;

	protected:

		// use this from within updateValueFromBlock() to set the value without making it authoritative
		void updateValue(value_assignment_t value)
		{
			mValue = value;
		}

		bool merge(BlockDescriptor& block_data, const BaseBlock& other, bool overwrite)
		{
			const derived_t& src_typed_param = static_cast<const derived_t&>(other);

			if (src_typed_param.mValueAge == VALUE_AUTHORITATIVE)
			{
				// copy value over
				setValue(src_typed_param.getValue());
				return true;
			}
			else
			{
				// merge individual parameters into destination
				return block_t::merge(block_t::selfBlockDescriptor(), src_typed_param, overwrite);
			}
		}

		mutable S32			mValidatedVersion;
		mutable bool 		mValidated; // lazy validation flag

	private:

		mutable T			mValue;
		mutable EValueAge	mValueAge;
	};
}


#endif // LL_LLPARAM_H
