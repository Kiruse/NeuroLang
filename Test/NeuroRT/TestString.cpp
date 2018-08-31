////////////////////////////////////////////////////////////////////////////////
// Unit Test of the Neuro::Buffer template class.
// -----
// Copyright (c) Kiruse 2018 Germany
#include <iostream>
#include "Assert.hpp"
#include "NeuroString.hpp"
#include "NeuroTestingFramework.hpp"
#include "StringBuilder.hpp"

namespace Neuro {
    template<typename T, typename Alloc>
    std::ostream& operator<<(std::ostream& lhs, const Buffer<T, Alloc>& rhs) {
        lhs << "[";
        if (rhs.length()) {
            if (StringBuilder::canFormat(rhs[0])) {
                StringBuilder builder;
                for (std::size_t i = 0; i < rhs.length(); ++i) {
                    builder << rhs[i];
                    if (i != rhs.length() - 1) builder << ',';
                }
                lhs << builder;
            }
            else {
                lhs << "some value";
                if (rhs.length() > 1) lhs << 's';
            }
        }
        return lhs << "]";
    }
    
    template<typename T, typename Alloc>
    bool operator==(const Buffer<T, Alloc>& lhs, const Buffer<T, Alloc>& rhs) {
        if (lhs.length() != rhs.length()) return false;
        for (std::size_t i = 0; i < lhs.length(); ++i) {
            if (lhs[i] != rhs[i]) return false;
        }
        return true;
    }
    template<typename T, typename Alloc>
    bool operator!=(const Buffer<T, Alloc>& lhs, const Buffer<T, Alloc>& rhs) {
        return !(lhs == rhs);
    }
}

void TestNeuroString() {
    using namespace std;
    using namespace Neuro;
    using namespace Neuro::Testing;
    
    section("Neuro::String", [](){
        test("Construction", [](){
            String str1("test"), str2(str1);
            
            NEURO_ASSERT_EXPR(str1.length()) == 4;
            NEURO_ASSERT_EXPR(str1[0]) == 't';
            NEURO_ASSERT_EXPR(str1[1]) == 'e';
            NEURO_ASSERT_EXPR(str1[2]) == 's';
            NEURO_ASSERT_EXPR(str1[3]) == 't';
            NEURO_ASSERT_EXPR(str1[4]) == 0;
            
            NEURO_ASSERT_EXPR(str2.length()) == 4;
            NEURO_ASSERT_EXPR(str2[0]) == 't';
            NEURO_ASSERT_EXPR(str2[1]) == 'e';
            NEURO_ASSERT_EXPR(str2[2]) == 's';
            NEURO_ASSERT_EXPR(str2[3]) == 't';
            NEURO_ASSERT_EXPR(str2[4]) == 0;
        });
        
        test("Assignment", [](){
            String base("test"), str;
            str = base;
            Assert::Value(str) == base;
            
            str = "foobar";
            Assert::Value(str) == "foobar";
        });
        
        test("Concatenation", [](){
            String str1 = "foo", str2 = "bar";
            NEURO_ASSERT_EXPR(str1 + str2) == "foobar";
            
            str1 += str2;
            Assert::Value(str1) == "foobar";
        });
        
        test("Insertion", [](){
            String str1 = "legendary", str2 = "...";
            
            str1.insert(5, "- wait for it -");
            Assert::Value(str1) == "legen- wait for it -dary";
            
            str1.insert(18, str2);
            Assert::Value(str1) == "legen- wait for it... -dary";
        });
        
        test("Find", [](){
            String base = "legendary", find = "nda";
            
            NEURO_ASSERT_EXPR(base.find('e')) == 1;
            NEURO_ASSERT_EXPR(base.find('e', 2)) == 3;
            NEURO_ASSERT_EXPR(base.find("end")) == 3;
            NEURO_ASSERT_EXPR(base.find(find)) == 4;
            NEURO_ASSERT_EXPR(base.findLast('e')) == 3;
            
            Assert::Value(base.findByPredicate([](const String& str, std::size_t index, char curr){
                return index != 0 && curr == str[index - 1];
            })) == npos;
        });
        
        test("Replacement", [](){
            String base = "foobaz", replacer = "fooped up beyond all repair";
            base.replace('z', 'r');
            Assert::Value(base) == "foobar";
            
            base.replaceAll('o', 'u');
            Assert::Value(base) == "fuubar";
            
            base.replace("uu", "u");
            Assert::Value(base) == "fubar";
            
            base.replace("fubar", replacer);
            Assert::Value(base) == replacer;
        });
        
        test("Substring", [](){
            String base = "The cow hopped over the moon.";
            NEURO_ASSERT_EXPR(base.substr(4, 10)) == "cow hopped";
        });
        
        test("Split", [](){
            String base = "The cow hopped over the moon.";
            Assert::Value(split(base, ' ')) == Buffer<String>({"The", "cow", "hopped", "over", "the", "moon."});
        });
        
        test("Join", [](){
            String base = "The cow hopped over the moon.";
            Assert::Value(join(split(base, ' '), ' ')) == base;
        });
    });
}
