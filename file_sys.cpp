// $Id: file_sys.cpp,v 1.5 2016-01-14 16:16:52-08 - - $

//By: David Stewart (daastewa@ucsc.edu)
//By: Christopher Hahn (cnhahn@ucsc.edu)

#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <string>
#include <map>

using namespace std;

#include "debug.h"
#include "file_sys.h"

int inode::next_inode_nr {1};

struct file_type_hash {
  size_t operator() (file_type type) const {
    return static_cast<size_t> (type);
  }
};

ostream& operator<< (ostream& out, file_type type) {
  static unordered_map<file_type,string,file_type_hash> hash {
    {file_type::PLAIN_TYPE, "PLAIN_TYPE"},
    {file_type::DIRECTORY_TYPE, "DIRECTORY_TYPE"},
  };
  return out << hash[type];
}

inode_state::inode_state() {
  DEBUGF ('i', "root = " << root << ", cwd = " << cwd
        << ", prompt = \"" << prompt() << "\"");

  //creates the root directory...
  this->root = make_shared<inode>(file_type::DIRECTORY_TYPE);
  this->cwd = root;
  this->root->getPath() = "/";
  string dot = ".";
  string dotdot = "..";
  this->root->getbase()->get_dirents().insert
      (std::pair<string, inode_ptr>(dot, root));
  this->root->getbase()->get_dirents().insert
      (std::pair<string, inode_ptr>(dotdot, root));
}

const string& inode_state::prompt() { return prompt_; }

ostream& operator<< (ostream& out, const inode_state& state) 
{
  out << "inode_state: root = " << state.root
    << ", cwd = " << state.cwd;
  return out;
}

inode::inode(file_type type): inode_nr (next_inode_nr++) 
{
  switch (type) {
    case file_type::PLAIN_TYPE:
      contents = make_shared<plain_file>();
      break;
    case file_type::DIRECTORY_TYPE:
      contents = make_shared<directory>();
      break;
  }
  DEBUGF ('i', "inode " << inode_nr << ", type = " << type);
}

int inode::get_inode_nr() const 
{
  DEBUGF ('i', "inode = " << inode_nr);
  return inode_nr;
}

file_error::file_error (const string& what):
  runtime_error (what) {
}

size_t plain_file::size() const {

  string s;
  int total = 0;

  for (unsigned int num = 0; num < data.size(); num++)
  {
    s = data[num];
    total += s.length();
  }

  return total;
}

void plain_file::readfile(const string& input) const {

  string contents_;

  for(unsigned int num = 0; num < this->data.size(); num++)
  {
    if (num == this->data.size() - 1)
      contents_ += data[num];
    else
      contents_ += data[num] + " ";
  }

  cout << contents_ << endl;
}

// to satisfy compiler
map<string,inode_ptr>& plain_file::get_dirents() {} 

void plain_file::writefile (const wordvec& words) {
  DEBUGF ('i', words);
  this->data.clear();
  this->data = words;
}

// to satisfy compiler
void plain_file::print_dirents() {} 

void plain_file::remove (const string&) {
  throw file_error ("is a plain file");
}

void plain_file::mkdir (const string&, inode_state&) {
  throw file_error ("is a plain file");
}

void plain_file::mkfile (const string&) {
  throw file_error ("is a plain file");
}

size_t directory::size() const {

  //DEBUGF ('i', "size = " << size);
  int size = dirents.size();
  return size;
}

void directory::readfile(const string& input) const 
{
  bool existence = false;
  inode_ptr node;
  string s;

  try 
  {
    for(auto it = dirents.begin(); it != dirents.end(); ++it)
    {
      if (it->first == input && 
          it->second->getbase()->getType() == "directory")
      {
        existence = false;
        throw file_error("Error: " + input + " is a directory");
      }
      else if (it->first == input && 
        it->second->getbase()->getType() != "directory")
      {
        existence = true;
        node = it->second;
        node->getbase()->readfile("");
      }
    }
    if (existence == false)
      throw file_error("cat: " + input + 
        ": No such file or directory");
  }
  catch(file_error& error)
  {
    complain() << error.what() << endl;
  }
}

void directory::writefile (const wordvec& words) 
{
  std::shared_ptr<inode> new_file = 
    make_shared<inode>(file_type::PLAIN_TYPE);
  new_file->getbase()->writefile(words);

  this->dirents.insert
    (std::pair<string,inode_ptr>(file_name, new_file));
  //this->dirents.emplace(file_name, new_file);
}

void directory::remove (const string& filename) 
{
  DEBUGF ('i', filename);
  //cout << "The thing going to be deleted is: " << filename << endl;
  map<string,inode_ptr>::iterator it;
  it = dirents.find(filename);
  dirents.erase(it);
}

void directory::mkdir (const string& dirname, inode_state& state) 
{
  DEBUGF ('i', dirname);
  string dot = ".";
  string dotdot = "..";
  string parent_name = "";
  wordvec contents;
  inode_ptr new_directory_ptr =
    make_shared<inode>(file_type::DIRECTORY_TYPE);

  //cout << "current directory inside mkdir: ";
  //state.getcwd()->printPath();
  //cout << endl;

  //cout << "dirname: " << dirname << endl;

  new_directory_ptr->set_parent_pointer(state.getcwd());
  new_directory_ptr->getbase()->set_name(dirname);

  inode_ptr parent_ptr = new_directory_ptr->get_parent_pointer();

  parent_name = parent_ptr->getPath();
  //give new directory parent access
  new_directory_ptr->getbase()->get_dirents().insert
    (std::pair<string, inode_ptr>(dotdot, parent_ptr)); 
  state.getcwd() = new_directory_ptr;
  //give directory current access
  new_directory_ptr->getbase()->get_dirents().insert
    (std::pair<string, inode_ptr>(dot, state.getcwd()));
  state.getcwd() = parent_ptr;

  if(parent_name != "/")
    //updates current path of newly created directory
    new_directory_ptr->getPath() += parent_name + "/" + dirname;
  else
    new_directory_ptr->getPath() += "/" + dirname;

  //added this new directory to list of things in current directory
  state.getcwd()->getbase()->get_dirents().insert
    (std::pair<string, inode_ptr>(dirname, new_directory_ptr)); 
}

void directory::mkfile (const string& filename) 
{
  this->file_name = filename;
  //cout << "filename initialization: " << file_name << endl;
}

void directory::print_dirents() 
{
  std::map<std::string, inode_ptr>::iterator it;
  for(it = dirents.begin(); it != dirents.end(); ++it)
  {
    if (it->second->getbase()->getType() == "directory" and 
        it->first != "." and it->first!= ".." )
      cout << "   " << it->second->get_node_number() << "   " 
        << it->second->getbase()->size() << "    " 
        << it->first + "/" << endl;
    else
      cout << "   " << it->second->get_node_number() << "   " <<   
        it->second->getbase()->size() << "    " << it->first << endl;
  }
}

map<string,inode_ptr>& directory::get_dirents() { return dirents; }
