#pragma once

#include <vector>
#include <memory>
#include <iostream>

#include "document.hpp"
#include "file.hpp"

namespace qthu
{

struct document_store
{
    std::vector< std::shared_ptr< document > > docs;

    document& load_from_stdin()
    {
        auto text = read_file( std::cin );
        return add_document( document::stdin_name, text );
    }

    document& load_from_file( std::string path )
    {
        return add_document( path, read_file( path ) );
    }

    document& add_document( std::string_view name, std::string text )
    {
        return *docs.emplace_back( std::make_shared< document >( name, text ) );
    }
};

} // namespace qthu
