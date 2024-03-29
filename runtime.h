#pragma once

#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace runtime {

    // �������� ���������� ���������� Mython
    class Context {
    public:
        // ���������� ����� ������ ��� ������ print
        virtual std::ostream& GetOutputStream() = 0;

    protected:
        ~Context() = default;
    };

    // ������� ����� ��� ���� �������� ����� Mython
    class Object {
    public:
        virtual ~Object() = default;
        // ������� � os ��� ������������� � ���� ������
        virtual void Print(std::ostream& os, Context& context) = 0;
    };

    // ����������� �����-������, ��������������� ��� �������� ������� � Mython-���������
    class ObjectHolder {
    public:
        // ������ ������ ��������
        ObjectHolder() = default;

        // ���������� ObjectHolder, ��������� �������� ���� T
        // ��� T - ���������� �����-��������� Object.
        // object ���������� ��� ������������ � ����
        template <typename T>
        [[nodiscard]] static ObjectHolder Own(T&& object) {
            return ObjectHolder(std::make_shared<T>(std::forward<T>(object)));
        }

        // ������ ObjectHolder, �� ��������� �������� (������ ������ ������)
        [[nodiscard]] static ObjectHolder Share(Object& object);
        // ������ ������ ObjectHolder, ��������������� �������� None
        [[nodiscard]] static ObjectHolder None();

        // ���������� ������ �� Object ������ ObjectHolder.
        // ObjectHolder ������ ���� ��������
        Object& operator*() const;

        Object* operator->() const;

        [[nodiscard]] Object* Get() const;

        // ���������� ��������� �� ������ ���� T ���� nullptr, ���� ������ ObjectHolder �� ��������
        // ������ ������� ����
        template <typename T>
        [[nodiscard]] T* TryAs() const {
            return dynamic_cast<T*>(this->Get());
        }

        // ���������� true, ���� ObjectHolder �� ����
        explicit operator bool() const;

    private:
        explicit ObjectHolder(std::shared_ptr<Object> data);
        void AssertIsValid() const;

        std::shared_ptr<Object> data_;
    };

    // ������-��������, �������� �������� ���� T
    template <typename T>
    class ValueObject : public Object {
    public:
        ValueObject(T v)  // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)
            : value_(v) {
        }

        void Print(std::ostream& os, [[maybe_unused]] Context& context) override {
            os << value_;
        }

        [[nodiscard]] const T& GetValue() const {
            return value_;
        }

    private:
        T value_;
    };

    // ������� ��������, ����������� ��� ������� � ��� ���������
    using Closure = std::unordered_map<std::string, ObjectHolder>;

    // ���������, ���������� �� � object ��������, ���������� � True
    // ��� �������� �� ���� �����, True � �������� ����� ������������ true. � ��������� ������� - false.
    bool IsTrue(const ObjectHolder& object);

    // ��������� ��� ���������� �������� ��� ��������� Mython
    class Executable {
    public:
        virtual ~Executable() = default;
        // ��������� �������� ��� ��������� ������ closure, ��������� context
        // ���������� �������������� �������� ���� None
        virtual ObjectHolder Execute(Closure& closure, Context& context) = 0;
    };

    // ��������� ��������
    using String = ValueObject<std::string>;
    // �������� ��������
    using Number = ValueObject<int>;

    // ���������� ��������
    class Bool : public ValueObject<bool> {
    public:
        using ValueObject<bool>::ValueObject;

        void Print(std::ostream& os, Context& context) override;
    };

    // ����� ������
    struct Method {
        // ��� ������
        std::string name;
        // ����� ���������� ���������� ������
        std::vector<std::string> formal_params;
        // ���� ������
        std::unique_ptr<Executable> body;
    };

    // �����
    class Class : public Object {
    public:
        // ������ ����� � ������ name � ������� ������� methods, �������������� �� ������ parent
        // ���� parent ����� nullptr, �� �������� ������� �����
        explicit Class(std::string name, std::vector<Method> methods, const Class* parent);

        // ���������� ��������� �� ����� name ��� nullptr, ���� ����� � ����� ������ �����������
        [[nodiscard]] const Method* GetMethod(const std::string& name) const;

        // ���������� ��� ������
        [[nodiscard]] const std::string& GetName() const;

        // ������� � os ������ "Class <��� ������>", �������� "Class cat"
        void Print(std::ostream& os, Context& context) override;
    public:
        std::string name_;
        std::vector<Method> methods_;
        const Class* parent_;
    };

    // ��������� ������
    class ClassInstance : public Object {
    public:
        explicit ClassInstance(const Class& cls);

        /*
         * ���� � ������� ���� ����� __str__, ������� � os ���������, ������������ ���� �������.
         * � ��������� ������ � os ��������� ����� �������.
         */
        void Print(std::ostream& os, Context& context) override;

        /*
         * �������� � ������� ����� method, ��������� ��� actual_args ����������.
         * �������� context ����� �������� ��� ���������� ������.
         * ���� �� ��� �����, �� ��� �������� �� �������� ����� method, ����� ����������� ����������
         * runtime_error
         */
        ObjectHolder Call(const std::string& method, const std::vector<ObjectHolder>& actual_args,
            Context& context);

        // ���������� true, ���� ������ ����� ����� method, ����������� argument_count ����������
        [[nodiscard]] bool HasMethod(const std::string& method, size_t argument_count) const;

        // ���������� ������ �� Closure, ���������� ���� �������
        [[nodiscard]] Closure& Fields();
        // ���������� ����������� ������ �� Closure, ���������� ���� �������
        [[nodiscard]] const Closure& Fields() const;
    private:
        const Class& class_;
        Closure fields_;
    };

    /*
     * ���������� true, ���� lhs � rhs �������� ���������� �����, ������ ��� �������� ���� Bool.
     * ���� lhs - ������ � ������� __eq__, ������� ���������� ��������� ������ lhs.__eq__(rhs),
     * ���������� � ���� Bool. ���� lhs � rhs ����� �������� None, ������� ���������� true.
     * � ��������� ������� ������� ����������� ���������� runtime_error.
     *
     * �������� context ����� �������� ��� ���������� ������ __eq__
     */
    bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context);

    /*
     * ���� lhs � rhs - �����, ������ ��� �������� bool, ������� ���������� ��������� �� ���������
     * ���������� <.
     * ���� lhs - ������ � ������� __lt__, ���������� ��������� ������ lhs.__lt__(rhs),
     * ���������� � ���� bool. � ��������� ������� ������� ����������� ���������� runtime_error.
     *
     * �������� context ����� �������� ��� ���������� ������ __lt__
     */
    bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context);
    // ���������� ��������, ��������������� Equal(lhs, rhs, context)
    bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context);
    // ���������� �������� lhs>rhs, ��������� ������� Equal � Less
    bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context);
    // ���������� �������� lhs<=rhs, ��������� ������� Equal � Less
    bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context);
    // ���������� ��������, ��������������� Less(lhs, rhs, context)
    bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context);

    // ��������-��������, ����������� � ������.
    // � ���� ��������� ���� ����� ���������������� � ��������� ����� ������ output
    struct DummyContext : Context {
        std::ostream& GetOutputStream() override {
            return output;
        }

        std::ostringstream output;
    };

    // ������� ��������, � ��� ����� ���������� � ����� output, ���������� � �����������
    class SimpleContext : public runtime::Context {
    public:
        explicit SimpleContext(std::ostream& output)
            : output_(output) {
        }

        std::ostream& GetOutputStream() override {
            return output_;
        }

    private:
        std::ostream& output_;
    };

}  // namespace runtime