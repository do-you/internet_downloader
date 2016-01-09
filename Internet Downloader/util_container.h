#ifndef UTIL_CONTAINER_H
#define UTIL_CONTAINER_H

template<typename node_ty>
class my_list
{
public:
	typedef typename node_ty* node_ptr;
	my_list()
	{
		head_node.next = &tail_node;
		head_node.pre = nullptr;

		tail_node.next = nullptr;
		tail_node.pre = &head_node;

		m_size = 0;
	}

	~my_list()
	{
		for (auto x = begin(); x != end(); )
		{
			auto temp = x;
			x = x->next;
			delete temp;
		}
	}
	void push_front(node_ptr new_node)
	{
		new_node->pre = &head_node;
		new_node->next = head_node.next;

		head_node.next->pre = new_node;
		head_node.next = new_node;

		++m_size;
	}
	void push_back(node_ptr new_node)
	{
		new_node->pre = tail_node.pre;
		new_node->next = &tail_node;

		tail_node.pre->next = new_node;
		tail_node.pre = new_node;

		++m_size;
	}
	node_ptr pop_front()
	{
		if (empty())
			return nullptr;

		assert(head_node.next != &tail_node);
		assert(tail_node.pre != &head_node);

		auto temp = head_node.next;
		head_node.next = head_node.next->next;

		return temp;
	}
	node_ptr pop_back()
	{
		if (empty())
			return nullptr;

		assert(head_node.next != &tail_node);
		assert(tail_node.pre != &head_node);

		auto temp = tail_node.pre;
		tail_node.pre = tail_node.pre->pre;

		return temp;
	}

	void erase(node_ptr node)
	{
		node->pre->next = node->next;
		node->next->pre = node->pre;

		node->next = node->pre = nullptr;

		--m_size;
	}

	void insert_after(node_ptr pos, node_ptr new_node)
	{
		new_node->next = pos->next;
		pos->next = new_node;

		++m_size;
	}
	void insert_before(node_ptr pos, node_ptr new_node)
	{
		pos->pre->next = new_node;
		new_node->next = pos;

		++m_size;
	}

	size_t size()
	{
		return m_size;
	}
	bool empty()
	{
		return m_size == 0;
	}
	node_ptr begin()
	{
		return head_node.next;
	}
	const node_ptr end()
	{
		return &tail_node;
	}

private:
	node_ty head_node, tail_node;
	size_t m_size;
};




#endif // UTIL_CONTAINER_H
