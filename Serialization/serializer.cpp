#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <cassert>
#include <concepts>

class Archive;

/**
 *  Interface for class that needs to be serialized. Execute operator<< from Archive on each variable that needs to be saved.
 */
struct ISerializable
{
    virtual void Serialize(Archive& archive) = 0;
};

/**
 *  Base Archive type. You should derive from this.
 */
template<typename T>
concept BasicType = std::integral<T> || std::floating_point<T>;

class Archive
{
public:
    enum Mode
    {
        Write
        , Read
        , SIZE
    };

public:
    Archive(Mode _mode);

    inline bool IsWrite() const { return (mode_ == Mode::Write); };
    inline bool IsRead() const { return (mode_ == Mode::Read); };

    virtual void operator<<(int& val) const = 0;
    virtual void operator<<(unsigned int& val) const = 0;
    virtual void operator<<(std::string& val) const = 0;

    template<BasicType T>
    void operator<<(std::vector<T>& val) const 
    {
        // Fast path for basic types.
        if(IsWrite())
        {
            unsigned int size = val.size();
            *this << size;
            SerializeBuffer(val.data(), val.size() * sizeof(T));
        }
        else
        {
            unsigned int size = 0;
            *this << size;

            val.resize(size);
            SerializeBuffer(val.data(), val.size() * sizeof(T));
        }
    };

    template<typename T>
    void operator<<(std::vector<T>& val) const 
    {
        // Elem by elem for non-basic types.
        if(IsWrite())
        {
            unsigned int size = val.size();
            *this << size;

            for(int idx = 0; idx < size; ++idx)
            {
                *this << val[idx];
            }
        }
        else
        {
            unsigned int size = 0;
            *this << size;
            val.resize(size);

            for(int idx = 0; idx < size; ++idx)
            {
                *this << val[idx];
            }
        }
    }

public:
    static void SerializeToFile(ISerializable& _obj, const std::string& _path);
    static void SerializeFromFile(ISerializable& _obj, const std::string& _path);

protected:
    virtual void SerializeBuffer(void* data, int bytes) const = 0;

private:
    Mode mode_;
};

Archive::Archive(Mode _mode):
    mode_{_mode}
{
}

/**
 *  Archive that will serialize to file. You can make net-packet archive or to some other data container.
 */
class FileArchive: public Archive
{
public:
    FileArchive(const std::string& _path, Archive::Mode _mode);
    ~FileArchive();

    void operator<<(int& val) const override;
    void operator<<(unsigned int& val) const override;
    void operator<<(std::string& val) const override;

protected:
    void SerializeBuffer(void* data, int bytes) const override;

private:
    inline bool IsStreamValid() const
    {
        return (file_ && *file_);
    }

private:
    std::string path_;
    std::unique_ptr<std::fstream> file_;
};

FileArchive::FileArchive(const std::string& _path, Archive::Mode _mode):
    Archive(_mode),
    path_{ _path }
{

    const int ios_mode = (_mode == Archive::Mode::Read ? std::ios::in : std::ios::out);
    file_ = std::unique_ptr<std::fstream>(new std::fstream(_path, ios_mode | std::ios::binary));
    if (!IsStreamValid()) {
        std::cout << "Failed to open file for writing.\n";
    }
}

FileArchive::~FileArchive()
{
    if (!IsStreamValid()) {
        file_->close();
    }
}

void FileArchive::operator<<(int& val) const
{
    if(!IsStreamValid()) return;

    if(IsWrite())
    {
        file_->write(reinterpret_cast<char*>(&val), sizeof(val));
    }
    else
    {
        file_->read(reinterpret_cast<char*>(&val), sizeof(val));
    }
}

void FileArchive::operator<<(unsigned int& val) const
{
    if(!IsStreamValid()) return;

    if(IsWrite())
    {
        file_->write(reinterpret_cast<char*>(&val), sizeof(val));
    }
    else
    {
        file_->read(reinterpret_cast<char*>(&val), sizeof(val));
    }
}

void FileArchive::operator<<(std::string& val) const
{
    if(!IsStreamValid()) return;

    if(IsWrite())
    {
        const int len = val.length() + 1;
        file_->write(reinterpret_cast<const char*>(&len), sizeof(len));
        file_->write(val.c_str(), len);
    }
    else
    {
        int len;
        file_->read(reinterpret_cast<char*>(&len), sizeof(len));

        val.resize(len, '\0');
        file_->read(&val[0], len);
    }
}   

void FileArchive::SerializeBuffer(void* data, int bytes) const
{
    if(!IsStreamValid()) return;

    if(IsWrite())
    {
        file_->write(reinterpret_cast<const char*>(&bytes), sizeof(bytes));
        file_->write(reinterpret_cast<const char*>(data), bytes);
    }
    else
    {
        int read_bytes = 0;
        file_->read(reinterpret_cast<char*>(&read_bytes), sizeof(read_bytes));
        assert(read_bytes == bytes);

        file_->read(reinterpret_cast<char*>(data), bytes);
    }
};

void Archive::SerializeToFile(ISerializable& _obj, const std::string& _path)
{
    FileArchive file(_path, Archive::Write);
    _obj.Serialize(file);
}

void Archive::SerializeFromFile(ISerializable& _obj, const std::string& _path)
{
    FileArchive file(_path, Archive::Read);
    _obj.Serialize(file);
}

////////////////////////////////////// EXAMPLE
struct Foo : public ISerializable
{
    int integer = 0;
    std::string str = "";

    std::vector<int> vector = {};
    std::vector<std::string> vector_str = {};

    void Serialize(Archive& arch) override
    {
        arch << integer;
        arch << str;
        arch << vector;
        arch << vector_str;
    }
};

int main()
{
    // Save Foo
    Foo f;
    f.integer = 2;
    f.str = "Hello";
    f.vector = {1, 2, 3};
    f.vector_str = {"a", "HelloHelloHelloHelloHelloHelloHelloHelloHelloHelloHelloHello!"};

    Archive::SerializeToFile(f, "hello.bin");
    printf("Wrote file!\n");

    // Retrieve Foo
    Foo ff;
    Archive::SerializeFromFile(ff, "hello.bin");
    printf("Read file!\n");
    printf("Data=[%d '%s' {", ff.integer, ff.str.c_str());
    for(auto& v : ff.vector)
    {
        printf("%d, ", v);
    }
    printf("}, {");
    for(auto& s : ff.vector_str)
    {
        printf("'%s', ", s.c_str());
    }
    printf("}]\n");

    return 0;
}